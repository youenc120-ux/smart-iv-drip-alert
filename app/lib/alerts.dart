import 'package:audioplayers/audioplayers.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// 系統通知與警報鈴聲
class Alerts {
  final _notes = FlutterLocalNotificationsPlugin();
  final _player = AudioPlayer();
  bool _playing = false;

  Future<void> init() async {
    try {
      await _notes.initialize(const InitializationSettings(
        android: AndroidInitializationSettings('@mipmap/ic_launcher'),
        iOS: DarwinInitializationSettings(),
      ));
      await _notes
          .resolvePlatformSpecificImplementation<
              AndroidFlutterLocalNotificationsPlugin>()
          ?.requestNotificationsPermission();
    } catch (e) {
      debugPrint('通知初始化失敗：$e');
    }

    try {
      await _player.setReleaseMode(ReleaseMode.loop);
      // Android 用鬧鐘音量；iOS 用 playback，手機切到靜音也會響
      await _player.setAudioContext(AudioContext(
        android: AudioContextAndroid(
          usageType: AndroidUsageType.alarm,
          contentType: AndroidContentType.sonification,
          audioFocus: AndroidAudioFocus.gainTransientMayDuck,
        ),
        iOS: AudioContextIOS(
          category: AVAudioSessionCategory.playback,
          options: const {},
        ),
      ));
    } catch (e) {
      debugPrint('音訊初始化失敗：$e');
    }
  }

  /// 同一床位的通知會取代舊的，不會一直堆疊
  Future<void> notify(String key, String title, String body,
      {required bool urgent}) async {
    try {
      await _notes.show(
        key.hashCode & 0x7fffffff,
        title,
        body,
        NotificationDetails(
          android: AndroidNotificationDetails(
            urgent ? 'iv_urgent' : 'iv_info',
            urgent ? '點滴緊急警報' : '點滴提醒',
            channelDescription: '點滴液量與裝置狀態通知',
            importance: urgent ? Importance.max : Importance.high,
            priority: Priority.high,
            category: AndroidNotificationCategory.alarm,
          ),
          iOS: const DarwinNotificationDetails(
            presentAlert: true,
            presentSound: true,
          ),
        ),
      );
    } catch (e) {
      debugPrint('通知失敗：$e');
    }
  }

  Future<void> startAlarm() async {
    if (_playing) return;
    _playing = true;
    try {
      await _player.play(AssetSource('alarm.wav'));
    } catch (e) {
      debugPrint('鈴聲播放失敗：$e');
    }
  }

  Future<void> stopAlarm() async {
    if (!_playing) return;
    _playing = false;
    await _player.stop();
  }
}
