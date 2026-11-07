# Voice وتشغيل الصوت

توفر واجهة Voice الاختيارية تشغيل ملفات صوتية محلية وHTTP audio streams باستخدام SDL2_mixer. هذه الواجهة ليست Text-to-Speech؛ فهي لا تحوّل النص إلى كلام ولا تسجل الصوت.

## المتطلبات

تحتاج إلى SDL2 وSDL2_mixer و§pkg-config§:

- Fedora: §sudo dnf install SDL2-devel SDL2_mixer-devel pkgconf-pkg-config§
- Debian أو Ubuntu: §sudo apt install libsdl2-dev libsdl2-mixer-dev pkg-config§
- macOS: §brew install sdl2 sdl2_mixer pkg-config§
- Windows: استخدم MSYS2 MINGW64 وحزم SDL2 وSDL2_mixer وpkgconf الخاصة بـ MinGW.

## البناء

§§§sh
make voice
make voice-example
§§§

## الدوال الأساسية

- §voice.init()§ لتهيئة SDL Audio وMixer.
- §voice.play(path)§ لتشغيل ملف صوتي محلي.
- §voice.play_stream(stream)§ لاستهلاك §http_stream§ عبر SDL_RWops.
- §voice.pause()§ و§voice.resume()§ للإيقاف المؤقت والاستئناف.
- §voice.stop()§ لإيقاف المسار وتحريره.
- §voice.is_playing()§ و§voice.is_paused()§ و§voice.is_finished()§ لمعرفة الحالة.
- §voice.set_volume(percent)§ لضبط الصوت من 0 إلى 100.
- §voice.last_error()§ لقراءة الخطأ، و§voice.shutdown()§ لتحرير الموارد.

## HTTP Streaming

تنقل §voice.play_stream(http_stream)§ ملكية HTTP Stream إلى Voice Runtime. بعد نجاح النقل، لا تستخدم Handle من جهة المستدعي مرة أخرى. يستخدم Decoder مخزناً مؤقتاً لدعم عمليات Seek التي قد يجريها SDL_mixer عند تحليل بعض الصيغ، بحد أقصى 256 MiB.

## القيود

تعتمد الصيغ المتاحة على Codecs المفعّلة في SDL2_mixer المثبتة. يدير Runtime مساراً موسيقياً واحداً. يجب تسلسل استدعاءات التشغيل ودورة الحياة من Thread مالك واحد، كما يجب أن تكون Stream callbacks آمنة للخيوط. لا توفر الواجهة تسجيل الصوت أو عدة مسارات متزامنة أو قوائم تشغيل.

C Backend هو المسار المدعوم لهذه الواجهة الاختيارية؛ NASM لا ينفذ دوال §voice.*§.