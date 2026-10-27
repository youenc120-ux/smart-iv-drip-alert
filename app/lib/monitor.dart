import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import 'alerts.dart';
import 'models.dart';

/// 輪詢所有床位、判斷狀態變化、觸發通知與鈴聲
class Monitor extends ChangeNotifier {
  final Alerts alerts = Alerts();
  final List<Device> devices = [];
  final Map<String, BedStatus> status = {};

  bool alarmSound = true;
  bool keepAwake = true;
  bool demo = false;

  final Map<String, int> _fails = {};
  final Map<String, DripState> _last = {};
  final Set<String> _acked = {}; // 已在 App 按下「確認」的床位
  late SharedPreferences _prefs;
  Timer? _timer;
  bool _polling = false;

  // 展示模式：不需硬體的模擬床位
  static final List<Device> _demoDevices = [
    Device('demo-1', '301 病房 A 床'),
    Device('demo-2', '301 病房 B 床'),
    Device('demo-3', '302 病房 A 床'),
  ];
  final Map<String, DripState> _demoState = {'demo-2': DripState.low};
  final Map<String, DateTime> _demoSince = {};

  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
    final saved = _prefs.getStringList('devices') ?? [];
    devices.addAll(saved
        .map((s) => Device.fromJson(jsonDecode(s) as Map<String, dynamic>)));
    alarmSound = _prefs.getBool('alarmSound') ?? true;
    keepAwake = _prefs.getBool('keepAwake') ?? true;
    demo = _prefs.getBool('demo') ?? false;

    await alerts.init();
    _applyWakelock();
    _timer = Timer.periodic(const Duration(seconds: 2), (_) => poll());
    poll();
  }

  List<Device> get visibleDevices => demo ? _demoDevices : devices;

  /// 最需要處理的床位排在最上面
  List<Device> get sortedDevices => [...visibleDevices]..sort((a, b) =>
      _severity(b).compareTo(_severity(a)));

  int _severity(Device d) =>
      status[d.id]?.state.severity ?? DripState.offline.severity;

  bool isAcked(String id) => _acked.contains(id);

  bool get alarming => visibleDevices.any((d) {
        final s = status[d.id];
        return s != null &&
            s.state == DripState.empty &&
            !s.muted &&
            !_acked.contains(d.id);
      });

  Future<void> poll() async {
    if (_polling) return;
    _polling = true;
    try {
      await Future.wait(visibleDevices.map(_pollOne));
      _updateAlarm();
      notifyListeners();
    } finally {
      _polling = false;
    }
  }

  Future<void> _pollOne(Device d) async {
    BedStatus? s;
    if (demo) {
      s = _demoStatus(d);
    } else {
      s = await fetchStatus(d);
      if (s == null) {
        final f = (_fails[d.id] ?? 0) + 1;
        _fails[d.id] = f;
        if (f < 3) return; // 偶爾掉一次封包不算離線
        s = status[d.id]?.withState(DripState.offline) ??
            BedStatus.offline(d.address);
      } else {
        _fails[d.id] = 0;
      }
    }
    status[d.id] = s;
    _onState(d, s);
  }

  static Future<BedStatus?> fetchStatus(Device d) async {
    try {
      final r =
          await http.get(d.uri('/status')).timeout(const Duration(seconds: 3));
      if (r.statusCode != 200) return null;
      return BedStatus.fromJson(
          jsonDecode(utf8.decode(r.bodyBytes)) as Map<String, dynamic>);
    } catch (_) {
      return null;
    }
  }

  void _onState(Device d, BedStatus s) {
    final prev = _last[d.id];
    if (prev == s.state) return;
    _last[d.id] = s.state;
    if (s.state != DripState.empty) _acked.remove(d.id);

    // 第一次連上且正常，不需通知
    if (prev == null && s.state == DripState.normal) return;

    if (s.state == DripState.normal) {
      if (prev == DripState.low || prev == DripState.empty) {
        alerts.notify(d.id, '✅ ${s.bed} 已恢復正常', '液位已恢復（已更換或已排除）',
            urgent: false);
      }
      return;
    }
    // 液位不會自己變多，耗盡 -> 偏低多半是液面晃動，不重複通知
    if (prev == DripState.empty && s.state == DripState.low) return;

    final icon = switch (s.state) {
      DripState.empty => '🚨',
      DripState.low => '⚠️',
      _ => '❗',
    };
    alerts.notify(d.id, '$icon ${s.bed}', '${s.state.label}，${s.state.hint}',
        urgent: s.state == DripState.empty);
  }

  void _updateAlarm() {
    if (alarmSound && alarming) {
      alerts.startAlarm();
    } else {
      alerts.stopAlarm();
    }
  }

  /// 護理師按下「確認」：停止 App 鈴聲，並讓現場蜂鳴器靜音
  Future<void> acknowledge(Device d) async {
    _acked.add(d.id);
    _updateAlarm();
    notifyListeners();
    if (!demo) {
      try {
        await http.post(d.uri('/mute')).timeout(const Duration(seconds: 3));
      } catch (_) {}
    }
    poll();
  }

  Future<void> addDevice(String address) async {
    devices.add(
        Device(DateTime.now().microsecondsSinceEpoch.toString(), address.trim()));
    await _save();
    poll();
  }

  Future<void> removeDevice(Device d) async {
    devices.removeWhere((x) => x.id == d.id);
    status.remove(d.id);
    _last.remove(d.id);
    _fails.remove(d.id);
    _acked.remove(d.id);
    await _save();
    _updateAlarm();
    notifyListeners();
  }

  Future<void> _save() => _prefs.setStringList(
      'devices', devices.map((d) => jsonEncode(d.toJson())).toList());

  void setAlarmSound(bool v) {
    alarmSound = v;
    _prefs.setBool('alarmSound', v);
    _updateAlarm();
    notifyListeners();
  }

  void setKeepAwake(bool v) {
    keepAwake = v;
    _prefs.setBool('keepAwake', v);
    _applyWakelock();
    notifyListeners();
  }

  void setDemo(bool v) {
    demo = v;
    _prefs.setBool('demo', v);
    status.clear();
    _last.clear();
    _fails.clear();
    _acked.clear();
    _updateAlarm();
    notifyListeners();
    poll();
  }

  void setDemoState(String id, DripState s) {
    _demoState[id] = s;
    _demoSince[id] = DateTime.now();
    poll();
  }

  BedStatus _demoStatus(Device d) {
    final s = _demoState[d.id] ?? DripState.normal;
    final since = _demoSince.putIfAbsent(d.id, DateTime.now);
    return BedStatus(
      bed: d.address,
      state: s,
      upper: s == DripState.normal || s == DripState.fault,
      lower: s == DripState.normal || s == DripState.low,
      muted: false,
      muteLeft: 0,
      since: DateTime.now().difference(since).inSeconds,
      updated: DateTime.now(),
    );
  }

  void _applyWakelock() {
    if (keepAwake) {
      WakelockPlus.enable();
    } else {
      WakelockPlus.disable();
    }
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}
