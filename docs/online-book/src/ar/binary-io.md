# الإدخال والإخراج الثنائي (Binary I/O)

تتعامل واجهة Binary I/O التجريبية مع البايتات الخام مباشرة، بدلاً من تمرير البيانات الثنائية عبر نصوص `string`.

## نوع byte والمصفوفات

يمثل النوع `byte` قيمة غير موقّعة من 8-bit ضمن المجال 0 إلى 255. استخدم `byte(value)` للتحويل من `int`؛ القيم خارج المجال تسبب خطأ وقت التشغيل.

```ylang
let byte header[] = [byte(0x59), byte(0x4c), byte(0x41), byte(0x4e)];
print(header[0]);
```

## واجهة الملفات

| الدالة | الوظيفة |
|---|---|
| `io.binary_size(path)` | حجم الملف بالبايت أو `-1` عند الفشل. |
| `io.read_u8(path, offset)` | قراءة بايت واحد أو `-1` عند الفشل أو EOF. |
| `io.write_u8(path, offset, value)` | كتابة بايت في Offset مطلق. |
| `io.read_bytes(path, offset, buffer)` | قراءة بايتات إلى المصفوفة وإرجاع العدد المقروء. |
| `io.write_bytes(path, offset, buffer)` | كتابة المصفوفة وإرجاع العدد المكتوب. |
| `io.read_binary_hex(path, offset, length)` | قراءة البيانات كنص Hex. |
| `io.write_binary_hex(path, offset, hex)` | فك Hex صالح بطول زوجي وكتابته. |

تبدأ Offsets من الصفر. قد تعيد القراءة عدداً أقل عند EOF. حد عمليات Hex هو 16 MiB لكل استدعاء.

## Dynamic byte buffers

عندما لا تعرف حجم البيانات مسبقاً، استخدم واجهة `io.buffer_*`. تكبر الـ Buffers تلقائياً، ويبلغ الحد الأقصى 256 MiB لكل Buffer، مع حد أقصى 64 Buffer مفتوحاً.

- `io.buffer_new(initial_capacity)` ينشئ Buffer قابلاً للنمو.
- `io.buffer_slice(handle, offset, length)` ينشئ View لا يملك الذاكرة على جزء من Buffer آخر.
- `io.buffer_length(handle)` و`io.buffer_capacity(handle)` يعيدان الطول والسعة.
- `io.buffer_get` و`io.buffer_set` لقراءة بايت أو استبداله.
- `io.buffer_push` لإضافة بايت في النهاية.
- `io.buffer_resize` لتغيير الطول، والبايتات الجديدة تصفّر.
- `io.buffer_clear` يجعل الطول صفراً مع الاحتفاظ بالسعة.
- `io.buffer_free` يحرر Buffer ويبطل Handle.
- `io.buffer_load` و`io.buffer_save` و`io.buffer_append_file` لنقل البيانات من وإلى الملفات.

تشارك الـ Slices ذاكرة الـ Parent. يمكن إنشاء Slices متداخلة؛ تحرير أي Buffer يبطل ذلك الـ Buffer وكل Slices التابعة له. لا تستخدم Handle بعد إبطاله.

## Endian وStreaming

توفر الواجهة عمليات قراءة وكتابة لأعداد 16-bit و32-bit و64-bit بصيغتي Little-endian وBig-endian، مثل `io.read_u32_le` و`io.write_u32_be`.

تسمح `io.stream_open_read` و`io.stream_open_write` بقراءة الملف أو كتابته على دفعات. تعيد `io.stream_read` عدد البايتات، أو صفراً عند EOF، أو `-1` عند الخطأ.

## الحدود الحالية

- Streaming متزامن ولا يوفر Seek أو Flush أو Append أو Async.
- Offsets الملفات يجب أن تناسب مجال `long` في المنصة.
- Binary I/O تجريبي ومدعوم عبر C Backend؛ NASM لا يدعم هذه الواجهة.
- `io.read_file` و`io.write_file` واجهتا نص وليستا بديلاً عن Binary I/O.

للتواقيع الدقيقة، راجع [مواصفات اللغة الإنجليزية](../reference/language-spec.md).