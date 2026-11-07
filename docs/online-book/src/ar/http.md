# HTTP وواجهات API

توفر YLang واجهة HTTP تجريبية عبر libcurl في C Backend. تشمل طلبات متزامنة، ومهاماً غير متزامنة محدودة، وPull-based Streaming.

## المتطلبات

تحتاج البرامج التي تستخدم `http.*` إلى ملفات تطوير libcurl و`pkg-config`:

- Fedora: `sudo dnf install libcurl-devel pkgconf-pkg-config`
- Debian/Ubuntu: `sudo apt install libcurl4-openssl-dev pkg-config`
- macOS: `brew install curl pkg-config`
- Windows MSYS2 MINGW64: `pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf`

البرامج التي لا تستخدم HTTP لا تحتاج libcurl. يستطيع `ylang check` فحص الاستدعاءات دون ربط Runtime الخاص بالشبكة، لكن البناء الأصلي يحتاج الاعتماديات.

## الطلبات المتزامنة

- `http.get(url)` و`http.get_with_headers(url, headers)` لطلبات GET.
- `http.post(url, body)` و`http.post_with_headers(...)` لطلبات POST.
- `http.request(method, url, body, headers)` للطرق GET وPOST وPUT وPATCH وDELETE وHEAD.

تُمرر Headers كسطر لكل Header، وتفصل بينها علامة `\n`. افحص `http.status()` لمعرفة رمز استجابة الخادم، و`http.last_error()` لمعرفة خطأ النقل أو الإعداد. استجابة 404 ليست فشل اتصال؛ إنها استجابة HTTP وصلتك بنجاح.

الطلبات المخزنة مؤقتاً محدودة بحجم 8 MiB، ووقت اتصال 5 ثوانٍ، ووقت إجمالي 15 ثانية.

## المهام غير المتزامنة

`http.request_async(method, url, body, headers)` ينشئ `http_task`. استخدم `await task` للحصول على جسم الاستجابة، أو `http.cancel(task)` لإلغاء مهمة لم تُستهلك بعد.

توجد 32 خانة مهام كحد أقصى. كل Task يمكن استهلاكه مرة واحدة فقط. الإلغاء يحرر حالة الطلب محلياً، ولا يتراجع عن طلب وصل بالفعل إلى الخادم. هذه ليست Async/Await عامة لكل دوال اللغة.

## HTTP Streaming

تُنشئ `http.stream_open(method, url, body, headers)` مقبض `http_stream` منفصلاً. اقرأ النص باستخدام `http.stream_read`، وتابع حالة النقل باستخدام `http.stream_done`، واقرأ الأخطاء عبر `http.stream_error`، ثم أغلق المقبض باستخدام `http.stream_close`.

- يوجد حد أقصى 16 Stream نشطاً.
- يستخدم كل Stream مخزناً مؤقتاً بحجم 64 KiB.
- لا يوجد وقت إجمالي ثابت للـ Streaming، حتى لا تُقطع التدفقات الطويلة.
- `http.stream_status` و`http.stream_headers` ينتظران بيانات الاستجابة النهائية ولا يستهلكان جسمها.
- `http.stream_read` مخصص للنص؛ إذا احتوت Chunk على NUL ينهي Stream ويسجل خطأ.
- للبيانات الثنائية استخدم `http.stream_read_buffer(stream, buffer, max_bytes)` مع Root Buffer من `io.buffer_new(0)`. يحافظ هذا المسار على NUL والبايتات ذات High Bit.

يمكن نقل ملكية Stream إلى `voice.play_stream(stream)`. بعد نجاح النقل، لا تستخدم المقبض من جهة المستدعي مجدداً.

## Redirect وTLS

تُقبل عناوين HTTP وHTTPS فقط، مع التحقق من شهادة TLS واسم المضيف. يمكن اتباع خمسة Redirects كحد أقصى عندما لا توجد Headers مخصصة. إذا أرسلت Header غير `Content-Type`، يتوقف التتبع التلقائي حتى لا تُرسل بيانات اعتماد مثل `Authorization` أو `X-API-Key` إلى وجهة أخرى.

لا يوجد خيار لتعطيل التحقق من TLS. مع ذلك، لا تجعل هذه الحماية كل URL آمناً؛ يجب على تطبيقات الخادم التحقق من الوجهة ومنع الوصول غير المقصود إلى Loopback أو Private أو Metadata IPs.

## مفاتيح API

اقرأ المفتاح من متغير بيئة باستخدام `env.get(name)`. تعيد الدالة نصاً فارغاً إذا كان المتغير غير موجود، ولا تشغل Shell.

لا تضع مفاتيح حقيقية في المصدر أو الأمثلة أو Git. راجع [مرجع HTTP الإنجليزي](../reference/HTTP.md) و[تفاصيل Streaming](../reference/HTTP-STREAMING.md).