#ifndef VOLUME_ENCODING_H
#define VOLUME_ENCODING_H

#include <QtGlobal>

// 容积数值对应编码的枚举与转换工具
// 使用示例：
//   quint16 code;
//   if (VolumeEncoding::tryEncode(100, code)) { /* code == 51266 */ }
//   int volume;
//   if (VolumeEncoding::tryDecode(static_cast<quint16>(VolumeEncoding::VolumeCode::V100), volume)) { /* volume == 100 */ }

namespace VolumeEncoding {

// 将“十进制容积数值”映射到“对应编码”的枚举。
// 枚举值为实际编码；枚举名中的数字为容积值。
enum class VolumeCode : quint16 {
    V0   = 0,
    V1   = 32831,
    V2   = 64,
    V3   = 16448,
    V4   = 32832,
    V5   = 41024,
    V6   = 49216,
    V7   = 57408,
    V8   = 65,
    V9   = 4161,
    V10  = 8257,
    V11  = 12353,
    V12  = 16449,
    V13  = 20545,
    V14  = 24641,
    V15  = 28737,
    V16  = 32833,
    V17  = 34881,
    V18  = 36929,
    V19  = 38977,
    V20  = 41025,
    V21  = 43073,
    V22  = 45121,
    V23  = 47169,
    V24  = 49217,
    V25  = 51265,
    V30  = 61505,
    V31  = 63553,
    V32  = 66,
    V33  = 1090,
    V34  = 2114,
    V35  = 3138,
    V36  = 4162,
    V37  = 5186,
    V38  = 6210,
    V39  = 7234,
    V40  = 8258,
    V41  = 9282,
    V42  = 10306,
    V43  = 11330,
    V44  = 12354,
    V45  = 13378,
    V46  = 14402,
    V47  = 15426,
    V48  = 16450,
    V49  = 17474,
    V50  = 18498,
    V60  = 28738,
    V70  = 35906,
    V80  = 41026,
    V90  = 46146,
    V100 = 51266,
    V110 = 56386,
    V120 = 61506,
    V130 = 579,
    V140 = 3139,
    V150 = 5699,
    V160 = 8259,
    V170 = 10819,
    V180 = 13379,
    V190 = 15939,
    V200 = 18499,
    V300 = 38467,
    V400 = 51267,
    V500 = 64067,

    Unknown = 0xFFFF
};

// 尝试将“容积值”编码为“对应编码”。成功返回 true，并通过 outCode 输出编码；失败返回 false。
inline bool tryEncode(int volume, quint16 &outCode) {
    switch (volume) {
    case 0:   outCode = static_cast<quint16>(VolumeCode::V0);   return true;
    case 1:   outCode = static_cast<quint16>(VolumeCode::V1);   return true;
    case 2:   outCode = static_cast<quint16>(VolumeCode::V2);   return true;
    case 3:   outCode = static_cast<quint16>(VolumeCode::V3);   return true;
    case 4:   outCode = static_cast<quint16>(VolumeCode::V4);   return true;
    case 5:   outCode = static_cast<quint16>(VolumeCode::V5);   return true;
    case 6:   outCode = static_cast<quint16>(VolumeCode::V6);   return true;
    case 7:   outCode = static_cast<quint16>(VolumeCode::V7);   return true;
    case 8:   outCode = static_cast<quint16>(VolumeCode::V8);   return true;
    case 9:   outCode = static_cast<quint16>(VolumeCode::V9);   return true;
    case 10:  outCode = static_cast<quint16>(VolumeCode::V10);  return true;
    case 11:  outCode = static_cast<quint16>(VolumeCode::V11);  return true;
    case 12:  outCode = static_cast<quint16>(VolumeCode::V12);  return true;
    case 13:  outCode = static_cast<quint16>(VolumeCode::V13);  return true;
    case 14:  outCode = static_cast<quint16>(VolumeCode::V14);  return true;
    case 15:  outCode = static_cast<quint16>(VolumeCode::V15);  return true;
    case 16:  outCode = static_cast<quint16>(VolumeCode::V16);  return true;
    case 17:  outCode = static_cast<quint16>(VolumeCode::V17);  return true;
    case 18:  outCode = static_cast<quint16>(VolumeCode::V18);  return true;
    case 19:  outCode = static_cast<quint16>(VolumeCode::V19);  return true;
    case 20:  outCode = static_cast<quint16>(VolumeCode::V20);  return true;
    case 21:  outCode = static_cast<quint16>(VolumeCode::V21);  return true;
    case 22:  outCode = static_cast<quint16>(VolumeCode::V22);  return true;
    case 23:  outCode = static_cast<quint16>(VolumeCode::V23);  return true;
    case 24:  outCode = static_cast<quint16>(VolumeCode::V24);  return true;
    case 25:  outCode = static_cast<quint16>(VolumeCode::V25);  return true;
    case 30:  outCode = static_cast<quint16>(VolumeCode::V30);  return true;
    case 31:  outCode = static_cast<quint16>(VolumeCode::V31);  return true;
    case 32:  outCode = static_cast<quint16>(VolumeCode::V32);  return true;
    case 33:  outCode = static_cast<quint16>(VolumeCode::V33);  return true;
    case 34:  outCode = static_cast<quint16>(VolumeCode::V34);  return true;
    case 35:  outCode = static_cast<quint16>(VolumeCode::V35);  return true;
    case 36:  outCode = static_cast<quint16>(VolumeCode::V36);  return true;
    case 37:  outCode = static_cast<quint16>(VolumeCode::V37);  return true;
    case 38:  outCode = static_cast<quint16>(VolumeCode::V38);  return true;
    case 39:  outCode = static_cast<quint16>(VolumeCode::V39);  return true;
    case 40:  outCode = static_cast<quint16>(VolumeCode::V40);  return true;
    case 41:  outCode = static_cast<quint16>(VolumeCode::V41);  return true;
    case 42:  outCode = static_cast<quint16>(VolumeCode::V42);  return true;
    case 43:  outCode = static_cast<quint16>(VolumeCode::V43);  return true;
    case 44:  outCode = static_cast<quint16>(VolumeCode::V44);  return true;
    case 45:  outCode = static_cast<quint16>(VolumeCode::V45);  return true;
    case 46:  outCode = static_cast<quint16>(VolumeCode::V46);  return true;
    case 47:  outCode = static_cast<quint16>(VolumeCode::V47);  return true;
    case 48:  outCode = static_cast<quint16>(VolumeCode::V48);  return true;
    case 49:  outCode = static_cast<quint16>(VolumeCode::V49);  return true;
    case 50:  outCode = static_cast<quint16>(VolumeCode::V50);  return true;
    case 60:  outCode = static_cast<quint16>(VolumeCode::V60);  return true;
    case 70:  outCode = static_cast<quint16>(VolumeCode::V70);  return true;
    case 80:  outCode = static_cast<quint16>(VolumeCode::V80);  return true;
    case 90:  outCode = static_cast<quint16>(VolumeCode::V90);  return true;
    case 100: outCode = static_cast<quint16>(VolumeCode::V100); return true;
    case 110: outCode = static_cast<quint16>(VolumeCode::V110); return true;
    case 120: outCode = static_cast<quint16>(VolumeCode::V120); return true;
    case 130: outCode = static_cast<quint16>(VolumeCode::V130); return true;
    case 140: outCode = static_cast<quint16>(VolumeCode::V140); return true;
    case 150: outCode = static_cast<quint16>(VolumeCode::V150); return true;
    case 160: outCode = static_cast<quint16>(VolumeCode::V160); return true;
    case 170: outCode = static_cast<quint16>(VolumeCode::V170); return true;
    case 180: outCode = static_cast<quint16>(VolumeCode::V180); return true;
    case 190: outCode = static_cast<quint16>(VolumeCode::V190); return true;
    case 200: outCode = static_cast<quint16>(VolumeCode::V200); return true;
    case 300: outCode = static_cast<quint16>(VolumeCode::V300); return true;
    case 400: outCode = static_cast<quint16>(VolumeCode::V400); return true;
    case 500: outCode = static_cast<quint16>(VolumeCode::V500); return true;
    default:  return false;
    }
}

// 尝试将“对应编码”解码为“容积值”。成功返回 true，并通过 outVolume 输出容积；失败返回 false。
inline bool tryDecode(quint16 code, int &outVolume) {
    switch (code) {
    case static_cast<quint16>(VolumeCode::V0):   outVolume = 0;   return true;
    case static_cast<quint16>(VolumeCode::V1):   outVolume = 1;   return true;
    case static_cast<quint16>(VolumeCode::V2):   outVolume = 2;   return true;
    case static_cast<quint16>(VolumeCode::V3):   outVolume = 3;   return true;
    case static_cast<quint16>(VolumeCode::V4):   outVolume = 4;   return true;
    case static_cast<quint16>(VolumeCode::V5):   outVolume = 5;   return true;
    case static_cast<quint16>(VolumeCode::V6):   outVolume = 6;   return true;
    case static_cast<quint16>(VolumeCode::V7):   outVolume = 7;   return true;
    case static_cast<quint16>(VolumeCode::V8):   outVolume = 8;   return true;
    case static_cast<quint16>(VolumeCode::V9):   outVolume = 9;   return true;
    case static_cast<quint16>(VolumeCode::V10):  outVolume = 10;  return true;
    case static_cast<quint16>(VolumeCode::V11):  outVolume = 11;  return true;
    case static_cast<quint16>(VolumeCode::V12):  outVolume = 12;  return true;
    case static_cast<quint16>(VolumeCode::V13):  outVolume = 13;  return true;
    case static_cast<quint16>(VolumeCode::V14):  outVolume = 14;  return true;
    case static_cast<quint16>(VolumeCode::V15):  outVolume = 15;  return true;
    case static_cast<quint16>(VolumeCode::V16):  outVolume = 16;  return true;
    case static_cast<quint16>(VolumeCode::V17):  outVolume = 17;  return true;
    case static_cast<quint16>(VolumeCode::V18):  outVolume = 18;  return true;
    case static_cast<quint16>(VolumeCode::V19):  outVolume = 19;  return true;
    case static_cast<quint16>(VolumeCode::V20):  outVolume = 20;  return true;
    case static_cast<quint16>(VolumeCode::V21):  outVolume = 21;  return true;
    case static_cast<quint16>(VolumeCode::V22):  outVolume = 22;  return true;
    case static_cast<quint16>(VolumeCode::V23):  outVolume = 23;  return true;
    case static_cast<quint16>(VolumeCode::V24):  outVolume = 24;  return true;
    case static_cast<quint16>(VolumeCode::V25):  outVolume = 25;  return true;
    case static_cast<quint16>(VolumeCode::V30):  outVolume = 30;  return true;
    case static_cast<quint16>(VolumeCode::V31):  outVolume = 31;  return true;
    case static_cast<quint16>(VolumeCode::V32):  outVolume = 32;  return true;
    case static_cast<quint16>(VolumeCode::V33):  outVolume = 33;  return true;
    case static_cast<quint16>(VolumeCode::V34):  outVolume = 34;  return true;
    case static_cast<quint16>(VolumeCode::V35):  outVolume = 35;  return true;
    case static_cast<quint16>(VolumeCode::V36):  outVolume = 36;  return true;
    case static_cast<quint16>(VolumeCode::V37):  outVolume = 37;  return true;
    case static_cast<quint16>(VolumeCode::V38):  outVolume = 38;  return true;
    case static_cast<quint16>(VolumeCode::V39):  outVolume = 39;  return true;
    case static_cast<quint16>(VolumeCode::V40):  outVolume = 40;  return true;
    case static_cast<quint16>(VolumeCode::V41):  outVolume = 41;  return true;
    case static_cast<quint16>(VolumeCode::V42):  outVolume = 42;  return true;
    case static_cast<quint16>(VolumeCode::V43):  outVolume = 43;  return true;
    case static_cast<quint16>(VolumeCode::V44):  outVolume = 44;  return true;
    case static_cast<quint16>(VolumeCode::V45):  outVolume = 45;  return true;
    case static_cast<quint16>(VolumeCode::V46):  outVolume = 46;  return true;
    case static_cast<quint16>(VolumeCode::V47):  outVolume = 47;  return true;
    case static_cast<quint16>(VolumeCode::V48):  outVolume = 48;  return true;
    case static_cast<quint16>(VolumeCode::V49):  outVolume = 49;  return true;
    case static_cast<quint16>(VolumeCode::V50):  outVolume = 50;  return true;
    case static_cast<quint16>(VolumeCode::V60):  outVolume = 60;  return true;
    case static_cast<quint16>(VolumeCode::V70):  outVolume = 70;  return true;
    case static_cast<quint16>(VolumeCode::V80):  outVolume = 80;  return true;
    case static_cast<quint16>(VolumeCode::V90):  outVolume = 90;  return true;
    case static_cast<quint16>(VolumeCode::V100): outVolume = 100; return true;
    case static_cast<quint16>(VolumeCode::V110): outVolume = 110; return true;
    case static_cast<quint16>(VolumeCode::V120): outVolume = 120; return true;
    case static_cast<quint16>(VolumeCode::V130): outVolume = 130; return true;
    case static_cast<quint16>(VolumeCode::V140): outVolume = 140; return true;
    case static_cast<quint16>(VolumeCode::V150): outVolume = 150; return true;
    case static_cast<quint16>(VolumeCode::V160): outVolume = 160; return true;
    case static_cast<quint16>(VolumeCode::V170): outVolume = 170; return true;
    case static_cast<quint16>(VolumeCode::V180): outVolume = 180; return true;
    case static_cast<quint16>(VolumeCode::V190): outVolume = 190; return true;
    case static_cast<quint16>(VolumeCode::V200): outVolume = 200; return true;
    case static_cast<quint16>(VolumeCode::V300): outVolume = 300; return true;
    case static_cast<quint16>(VolumeCode::V400): outVolume = 400; return true;
    case static_cast<quint16>(VolumeCode::V500): outVolume = 500; return true;
    default:  return false;
    }
}

} // namespace VolumeEncoding

#endif // VOLUME_ENCODING_H
