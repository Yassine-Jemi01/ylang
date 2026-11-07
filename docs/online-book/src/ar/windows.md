# بناء YLang على Windows

دعم Windows الأصلي في YLang تجريبي ويستهدف Windows 64-bit باستخدام MinGW-w64. لا يعني نجاح Smoke Tests أن كل ميزات اللغة لها تطابق كامل مع Linux أو macOS.

## 1. تثبيت MSYS2

ثبّت MSYS2 من [الموقع الرسمي](https://www.msys2.org/)، ثم افتح طرفية **MSYS2 MINGW64**، وليس طرفية MSYS العادية.

حدّث الحزم وثبّت الأدوات:

```sh
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf make python
```

تأكد من وجود مجلد MinGW64 في PATH عند استخدام YLang من طرفية Windows:

```text
C:\msys64\mingw64\bin
```

قد يختلف المسار إذا ثبّت MSYS2 في مكان آخر.

## 2. بناء المترجم

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make clean all CC=gcc
./build/ylang.exe --version
```

شغّل اختبارات Smoke الخاصة بالمنصة عند توفر اعتمادياتها. لا تفترض أن كامل مجموعة `make test` المحمولة على POSIX تعمل على Windows؛ يستخدم CI اختبارات Windows أصلية مركزة، ومنها بناء وتشغيل برنامج واستيراد ملفات واختبار الإدخال وTUI وHTTP وYLanglings.

```sh
make ylanglings-test ylanglings-verify PYTHON=python
```

## 3. بناء برنامج أصلي

```ylang
function main() -> int {
    print("Hello, Windows");
    print("Answer:", 40 + 2);
    return 0;
}
```

احفظه في `hello.yl` ثم:

```sh
./build/ylang.exe check hello.yl
./build/ylang.exe build hello.yl -o hello.exe
./hello.exe
```

يولد C Backend كود C ويستدعي GCC المتاح في PATH.

## 4. الاستيراد والمسارات

تعمل الاستيرادات النسبية انطلاقاً من الملف المستورد منه:

```ylang
from "./math.yl" import answer;
```

تُعامل مسارات Windows دون حساسية لحالة الأحرف عند إزالة تكرار الوحدات. متغير البيئة `YLANG_PATH` يستخدم فاصلة منقوطة للفصل بين المسارات.

في طرفية MSYS2 استخدم صيغة الشرطات المائلة:

```sh
export YLANG_PATH='C:/dev/ylang-libs;D:/shared/ylang'
```

في PowerShell:

```powershell
$env:YLANG_PATH = "C:/dev/ylang-libs;D:/shared/ylang"
```

## 5. GUI وGame2D وHTTP وVoice

تحتاج GUI وGame2D إلى SDL2 وملفات التطوير و`pkg-config`. تحتاج HTTP إلى حزمة libcurl الخاصة بـ MinGW-w64. أما Voice فتحتاج SDL2_mixer أيضاً.

لا تحتاج برامج الطرفية العادية إلى هذه المكتبات الاختيارية. راجع [دليل Windows الإنجليزي](../reference/WINDOWS.md) لقائمة القيود الحالية.

## القيود الحالية

- MSVC غير مدعوم في مسار البناء هذا؛ استخدم MinGW-w64 GCC.
- NASM Backend تجريبي ومخصص لـ Linux x86-64.
- التصحيح التفاعلي غير متاح على Windows حالياً.
- بعض اختبارات POSIX وسكربتات التثبيت تحتاج إلى متابعة خاصة بالمنصة.
- التثبيت النظامي وتجميع كل الاعتماديات الاختيارية على Windows ليسا جزءاً من هذا المسار.