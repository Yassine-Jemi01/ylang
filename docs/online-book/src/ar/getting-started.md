# تثبيت YLang

هذا الدليل يشرح بناء المترجم من المصدر. تحتاج إلى Git وGNU Make ومترجم C يدعم C17 مثل GCC أو Clang.

## Fedora

ثبّت الأدوات المطلوبة:

```sh
sudo dnf install gcc clang make git
```

## Debian أو Ubuntu

```sh
sudo apt update
sudo apt install build-essential clang make git
```

## macOS

ثبّت أدوات Apple ثم GNU Make وLLVM:

```sh
xcode-select --install
brew install make llvm git
```

على macOS استخدم `gmake` بدلاً من `make` في أوامر البناء والاختبار.

## تنزيل المصدر وبناء المترجم

نفّذ الأوامر التالية:

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
```

بعد نجاح البناء ستجد المترجم في `build/ylang`.

اختبر المشروع:

```sh
make test
```

لا تعتبر البناء ناجحاً إذا فشلت الاختبارات، حتى لو ظهر الملف التنفيذي.

## التحقق من المترجم

اعرض المساعدة:

```sh
./build/ylang help
```

ولعرض الإصدار:

```sh
./build/ylang version
```

## متطلبات اختيارية

أمثلة GUI وGame2D تحتاج مكتبات SDL2 و`pkg-config`. هذه المكتبات ليست مطلوبة لبرامج الطرفية العادية.

على Fedora:

```sh
sudo dnf install SDL2-devel pkgconf-pkg-config
```

على Debian أو Ubuntu:

```sh
sudo apt install libsdl2-dev pkg-config
```

## مشاكل شائعة

- إذا لم يجد النظام `make`، ثبّت GNU Make.
- إذا لم يجد GCC أو Clang، تأكد من تثبيت أحدهما ووجوده في `PATH`.
- إذا فشل SDL2، ثبّت حزم التطوير وليس مكتبة التشغيل فقط.
- عند فشل اختبار، احتفظ بأول رسالة فشل كاملة؛ الرسائل اللاحقة قد تكون نتيجة لها.

## التالي

بعد تجهيز المترجم، انتقل إلى [برنامجك الأول](hello-world.md).
