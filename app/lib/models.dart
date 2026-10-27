import 'package:flutter/material.dart';

enum DripState { normal, low, empty, fault, offline }

DripState parseDripState(String? s) => switch (s) {
      'normal' => DripState.normal,
      'low' => DripState.low,
      'empty' => DripState.empty,
      'fault' => DripState.fault,
      _ => DripState.offline,
    };

extension DripStateInfo on DripState {
  String get label => switch (this) {
        DripState.normal => '液量充足',
        DripState.low => '第一階段：液量偏低',
        DripState.empty => '第二階段：請立即更換',
        DripState.fault => '感測器異常',
        DripState.offline => '裝置離線',
      };

  String get hint => switch (this) {
        DripState.normal => '持續監測中',
        DripState.low => '請準備更換點滴',
        DripState.empty => '請立即更換點滴！',
        DripState.fault => '請檢查感測器安裝位置',
        DripState.offline => '請確認裝置電源與 Wi-Fi',
      };

  Color get color => switch (this) {
        DripState.normal => const Color(0xFF2E9D5B),
        DripState.low => const Color(0xFFE09400),
        DripState.empty => const Color(0xFFD63031),
        DripState.fault => const Color(0xFF6C5CE7),
        DripState.offline => const Color(0xFF8A94A3),
      };

  /// 排序與摘要用：數字越大越需要優先處理
  int get severity => switch (this) {
        DripState.normal => 0,
        DripState.fault => 1,
        DripState.offline => 2,
        DripState.low => 3,
        DripState.empty => 4,
      };

  /// 點滴袋圖示的液面高度（0–1）
  double get level => switch (this) {
        DripState.normal => 0.8,
        DripState.low => 0.45,
        DripState.empty => 0.12,
        DripState.fault || DripState.offline => 0.45,
      };

  bool get needsAction => this == DripState.low || this == DripState.empty;
}

/// 一台病房端 ESP32 裝置
class Device {
  Device(this.id, this.address);

  final String id;
  final String address; // 例如 192.168.1.50

  Uri uri(String path) {
    var a = address.trim();
    if (!a.startsWith('http')) a = 'http://$a';
    return Uri.parse(a).replace(path: path);
  }

  Map<String, dynamic> toJson() => {'id': id, 'address': address};

  factory Device.fromJson(Map<String, dynamic> j) =>
      Device(j['id'] as String, j['address'] as String);
}

/// ESP32 /status 回傳的狀態
class BedStatus {
  const BedStatus({
    required this.bed,
    required this.state,
    required this.upper,
    required this.lower,
    required this.muted,
    required this.muteLeft,
    required this.since,
    required this.updated,
  });

  final String bed;
  final DripState state;
  final bool upper;
  final bool lower;
  final bool muted;
  final int muteLeft;
  final int since; // 目前狀態已持續秒數
  final DateTime updated; // 最後一次成功取得資料的時間

  factory BedStatus.fromJson(Map<String, dynamic> j) => BedStatus(
        bed: j['bed'] as String? ?? '未命名床位',
        state: parseDripState(j['state'] as String?),
        upper: j['upper'] == true,
        lower: j['lower'] == true,
        muted: j['muted'] == true,
        muteLeft: (j['muteLeft'] as num?)?.toInt() ?? 0,
        since: (j['since'] as num?)?.toInt() ?? 0,
        updated: DateTime.now(),
      );

  factory BedStatus.offline(String bed) => BedStatus(
        bed: bed,
        state: DripState.offline,
        upper: false,
        lower: false,
        muted: false,
        muteLeft: 0,
        since: 0,
        updated: DateTime.now(),
      );

  BedStatus withState(DripState s) => BedStatus(
        bed: bed,
        state: s,
        upper: upper,
        lower: lower,
        muted: muted,
        muteLeft: muteLeft,
        since: since,
        updated: updated,
      );
}
