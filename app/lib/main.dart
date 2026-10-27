import 'package:flutter/material.dart';

import 'models.dart';
import 'monitor.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final monitor = Monitor();
  await monitor.init();
  runApp(IvApp(monitor: monitor));
}

class IvApp extends StatelessWidget {
  const IvApp({super.key, required this.monitor});

  final Monitor monitor;

  @override
  Widget build(BuildContext context) {
    const seed = Color(0xFF1565C0);
    return MaterialApp(
      title: '點滴監控',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(colorSchemeSeed: seed, useMaterial3: true),
      darkTheme: ThemeData(
          colorSchemeSeed: seed,
          brightness: Brightness.dark,
          useMaterial3: true),
      home: HomePage(monitor: monitor),
    );
  }
}

class HomePage extends StatelessWidget {
  const HomePage({super.key, required this.monitor});

  final Monitor monitor;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: monitor,
      builder: (context, _) {
        final list = monitor.sortedDevices;
        return Scaffold(
          appBar: AppBar(
            title: const Text('點滴監控'),
            actions: [
              if (monitor.demo)
                const Padding(
                  padding: EdgeInsets.only(right: 4),
                  child: Chip(label: Text('展示模式')),
                ),
              IconButton(
                icon: const Icon(Icons.settings_outlined),
                tooltip: '設定',
                onPressed: () => showSettings(context, monitor),
              ),
            ],
          ),
          floatingActionButton: monitor.demo
              ? null
              : FloatingActionButton.extended(
                  onPressed: () => addDeviceDialog(context, monitor),
                  icon: const Icon(Icons.add),
                  label: const Text('新增床位'),
                ),
          body: list.isEmpty
              ? EmptyView(monitor: monitor)
              : ListView(
                  padding: const EdgeInsets.fromLTRB(16, 8, 16, 96),
                  children: [
                    SummaryCard(monitor: monitor),
                    for (final d in list) BedCard(monitor: monitor, device: d),
                    if (monitor.demo)
                      const Padding(
                        padding: EdgeInsets.only(top: 16),
                        child: Text('展示模式：點一下床位可切換模擬狀態',
                            textAlign: TextAlign.center),
                      ),
                  ],
                ),
        );
      },
    );
  }
}

/// 最上方的總覽：幾床需要處理
class SummaryCard extends StatelessWidget {
  const SummaryCard({super.key, required this.monitor});

  final Monitor monitor;

  @override
  Widget build(BuildContext context) {
    final states = monitor.visibleDevices
        .map((d) => monitor.status[d.id]?.state)
        .whereType<DripState>()
        .toList();

    final String title;
    var worst = DripState.offline;
    if (states.isEmpty) {
      title = '連線中…';
    } else {
      worst = states.reduce((a, b) => a.severity >= b.severity ? a : b);
      final empty = states.where((s) => s == DripState.empty).length;
      final low = states.where((s) => s == DripState.low).length;
      final other = states
          .where((s) => s == DripState.fault || s == DripState.offline)
          .length;
      title = empty + low + other == 0
          ? '全部 ${states.length} 床正常'
          : [
              if (empty > 0) '$empty 床耗盡',
              if (low > 0) '$low 床偏低',
              if (other > 0) '$other 床異常',
            ].join('、');
    }

    return Card(
      color: worst.color,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Icon(
              worst == DripState.normal
                  ? Icons.check_circle
                  : Icons.notifications_active,
              color: Colors.white,
              size: 34,
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title,
                      style: const TextStyle(
                          color: Colors.white,
                          fontSize: 20,
                          fontWeight: FontWeight.bold)),
                  const SizedBox(height: 2),
                  Text(
                    monitor.alarming ? '警報響鈴中，請按「確認」' : '每 2 秒自動更新',
                    style: const TextStyle(color: Colors.white70),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class BedCard extends StatelessWidget {
  const BedCard({super.key, required this.monitor, required this.device});

  final Monitor monitor;
  final Device device;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final s = monitor.status[device.id];
    final state = s?.state ?? DripState.offline;
    final c = state.color;
    final showAck = state.needsAction &&
        !monitor.isAcked(device.id) &&
        !(s?.muted ?? false);

    return Card(
      clipBehavior: Clip.antiAlias,
      margin: const EdgeInsets.only(top: 12),
      child: InkWell(
        onTap: monitor.demo ? () => _demoPicker(context) : null,
        onLongPress: monitor.demo ? null : () => _confirmDelete(context),
        child: IntrinsicHeight(
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Container(width: 6, color: c),
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.all(14),
                  child: Row(
                    children: [
                      SizedBox(
                        width: 40,
                        height: 62,
                        child: CustomPaint(
                          painter: BagPainter(
                            level: state.level,
                            color: c,
                            outline: theme.colorScheme.outline,
                            grey: state == DripState.fault ||
                                state == DripState.offline,
                          ),
                        ),
                      ),
                      const SizedBox(width: 14),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(s?.bed ?? device.address,
                                style: theme.textTheme.titleMedium
                                    ?.copyWith(fontWeight: FontWeight.bold)),
                            const SizedBox(height: 2),
                            Text(state.label,
                                style: TextStyle(
                                    color: c,
                                    fontWeight: FontWeight.bold,
                                    fontSize: 16)),
                            const SizedBox(height: 4),
                            Text(_detail(s, state),
                                style: theme.textTheme.bodySmall),
                            if (s != null && state != DripState.offline)
                              Padding(
                                padding: const EdgeInsets.only(top: 8),
                                child: Wrap(
                                  spacing: 12,
                                  children: [
                                    _sensor('上位', s.upper),
                                    _sensor('下位', s.lower),
                                  ],
                                ),
                              ),
                          ],
                        ),
                      ),
                      if (showAck)
                        FilledButton(
                          style: FilledButton.styleFrom(backgroundColor: c),
                          onPressed: () => monitor.acknowledge(device),
                          child: const Text('確認'),
                        ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  String _detail(BedStatus? s, DripState state) {
    if (s == null) return '連線中… ${device.address}';
    if (state == DripState.offline) return '最後更新 ${_clock(s.updated)}';
    final note = s.muted || monitor.isAcked(device.id) ? '已確認' : state.hint;
    return '已持續 ${_duration(s.since)} · $note';
  }

  Widget _sensor(String name, bool liquid) => Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.circle,
              size: 10,
              color: liquid
                  ? DripState.normal.color
                  : DripState.empty.color),
          const SizedBox(width: 4),
          Text('$name${liquid ? '有液' : '無液'}',
              style: const TextStyle(fontSize: 13)),
        ],
      );

  Future<void> _demoPicker(BuildContext context) async {
    final picked = await showModalBottomSheet<DripState>(
      context: context,
      builder: (ctx) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const ListTile(title: Text('切換模擬狀態')),
            for (final st in DripState.values)
              ListTile(
                leading: Icon(Icons.circle, color: st.color),
                title: Text(st.label),
                onTap: () => Navigator.pop(ctx, st),
              ),
          ],
        ),
      ),
    );
    if (picked != null) monitor.setDemoState(device.id, picked);
  }

  Future<void> _confirmDelete(BuildContext context) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('移除床位？'),
        content: Text('將不再監看 ${monitor.status[device.id]?.bed ?? device.address}'),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('取消')),
          FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: const Text('移除')),
        ],
      ),
    );
    if (ok == true) monitor.removeDevice(device);
  }
}

String _two(int n) => n.toString().padLeft(2, '0');

String _clock(DateTime t) =>
    '${_two(t.hour)}:${_two(t.minute)}:${_two(t.second)}';

String _duration(int s) {
  final m = s ~/ 60;
  if (m >= 60) return '${m ~/ 60} 小時 ${m % 60} 分';
  if (m > 0) return '$m 分 ${s % 60} 秒';
  return '$s 秒';
}

/// 小型點滴袋圖示
class BagPainter extends CustomPainter {
  BagPainter({
    required this.level,
    required this.color,
    required this.outline,
    required this.grey,
  });

  final double level;
  final Color color;
  final Color outline;
  final bool grey;

  @override
  void paint(Canvas canvas, Size size) {
    final body = RRect.fromRectAndRadius(
        Rect.fromLTWH(2, 8, size.width - 4, size.height - 18),
        const Radius.circular(10));
    final fillTop = body.top + body.height * (1 - level);

    canvas.save();
    canvas.clipRRect(body);
    canvas.drawRect(
      Rect.fromLTRB(body.left, fillTop, body.right, body.bottom),
      Paint()
        ..color = (grey ? outline : color).withValues(alpha: grey ? 0.3 : 0.55),
    );
    canvas.restore();

    final stroke = Paint()
      ..color = outline
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;
    canvas.drawRRect(body, stroke);
    canvas.drawRect(Rect.fromLTWH(size.width / 2 - 5, 1, 10, 7), stroke);
    canvas.drawLine(Offset(size.width / 2, body.bottom),
        Offset(size.width / 2, size.height), stroke);
  }

  @override
  bool shouldRepaint(BagPainter old) =>
      old.level != level || old.color != color || old.grey != grey;
}

class EmptyView extends StatelessWidget {
  const EmptyView({super.key, required this.monitor});

  final Monitor monitor;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.water_drop_outlined,
                size: 64, color: theme.colorScheme.primary),
            const SizedBox(height: 16),
            Text('尚未加入任何床位', style: theme.textTheme.titleLarge),
            const SizedBox(height: 8),
            const Text(
              '手機和點滴監測裝置連上同一個 Wi-Fi 後，\n輸入裝置位址就能開始監看。',
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: () => addDeviceDialog(context, monitor),
              icon: const Icon(Icons.add),
              label: const Text('新增床位'),
            ),
            const SizedBox(height: 8),
            OutlinedButton(
              onPressed: () => monitor.setDemo(true),
              child: const Text('先試用展示模式'),
            ),
          ],
        ),
      ),
    );
  }
}

Future<void> addDeviceDialog(BuildContext context, Monitor monitor) async {
  final ctrl = TextEditingController();
  final addr = await showDialog<String>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: const Text('新增床位'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          TextField(
            controller: ctrl,
            autofocus: true,
            keyboardType: TextInputType.url,
            decoration: const InputDecoration(
                labelText: '裝置位址', hintText: '192.168.1.50'),
            onSubmitted: (v) => Navigator.pop(ctx, v.trim()),
          ),
          const SizedBox(height: 12),
          Text('位址會顯示在 ESP32 的序列埠監控視窗。\n熱點模式請填 192.168.4.1。',
              style: Theme.of(ctx).textTheme.bodySmall),
        ],
      ),
      actions: [
        TextButton(
            onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
        FilledButton(
            onPressed: () => Navigator.pop(ctx, ctrl.text.trim()),
            child: const Text('加入')),
      ],
    ),
  );
  if (addr == null || addr.isEmpty) return;

  final ok = await Monitor.fetchStatus(Device('', addr)) != null;
  await monitor.addDevice(addr);
  if (!context.mounted) return;
  ScaffoldMessenger.of(context).showSnackBar(SnackBar(
    content: Text(ok ? '已加入並連線成功' : '已加入，但目前連不到裝置，請確認位址與 Wi-Fi'),
  ));
}

void showSettings(BuildContext context, Monitor monitor) {
  showModalBottomSheet<void>(
    context: context,
    showDragHandle: true,
    builder: (ctx) => ListenableBuilder(
      listenable: monitor,
      builder: (ctx, _) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            SwitchListTile(
              title: const Text('第二階段警報鈴聲'),
              subtitle: const Text('用鬧鐘音量播放，手機靜音也會響'),
              value: monitor.alarmSound,
              onChanged: monitor.setAlarmSound,
            ),
            SwitchListTile(
              title: const Text('保持螢幕常亮'),
              subtitle: const Text('避免手機休眠導致停止更新'),
              value: monitor.keepAwake,
              onChanged: monitor.setKeepAwake,
            ),
            SwitchListTile(
              title: const Text('展示模式'),
              subtitle: const Text('不需硬體，用模擬床位展示'),
              value: monitor.demo,
              onChanged: monitor.setDemo,
            ),
            const ListTile(
              leading: Icon(Icons.info_outline),
              title: Text('使用須知'),
              subtitle: Text('App 開著時才會持續監測。長時間背景執行時，'
                  '系統可能暫停 App，請搭配裝置的 ntfy / Telegram 推播。\n'
                  '本系統為專題原型，非醫療器材。'),
            ),
          ],
        ),
      ),
    ),
  );
}
