#include <janet.h>
#include <sys/types.h>

static void put_varuint(JanetBuffer *buf, uint64_t x) {
  while (x >= 0x80) {
    janet_buffer_push_u8(buf, (uint8_t)x | 0x80);
    x >>= 7;
  }
  janet_buffer_push_u8(buf, (uint8_t)x);
}

static int decode_varuint(uint8_t *buf, size_t sz, size_t *offset,
                          uint64_t *out) {
  int i = 0;
  int s = 0;
  uint64_t v = 0;
  while (1) {
    if (sz < *offset + 1)
      return 0;
    uint8_t b = buf[*offset];
    *offset += 1;
    if (b < 0x80) {
      if (i > 9 || (i == 9 && b > 1))
        return 0;
      v |= (uint64_t)b << s;
      break;
    } else {
      v |= ((uint64_t)(b & 0x7f)) << s;
      s += 7;
    }
    i++;
  }
  *out = v;
  return 1;
}

static void unexpected_end_of_buffer_panic() {
  janet_panic("unexpected end of buffer");
}

static void could_not_decode_panic() {
  janet_panic("could not decode BARE message");
}

static void message_too_large_panic() {
  janet_panic("BARE message too large for implementation");
}

static uint64_t janet_to_uint64(Janet value) {
  uint64_t x = 0;

  if (janet_checktype(value, JANET_NUMBER)) {
    x = (uint64_t)janet_unwrap_number(value);
  } else {
    switch (janet_is_int(value)) {
    case JANET_INT_S64:
      x = (uint64_t)janet_unwrap_s64(value);
      break;
    case JANET_INT_U64:
      x = (uint64_t)janet_unwrap_u64(value);
      break;
    default:
      janet_panicf("unsupported value for numeric rule - %v", value);
    }
  }

  return x;
}

static int32_t decode_size_varuint(uint8_t *buf, size_t sz, size_t *offset) {
  uint64_t len;
  if (!decode_varuint(buf, sz, offset, &len))
    could_not_decode_panic();
  if (len > 0x7fffffff)
    message_too_large_panic();
  return (int32_t)len;
}

static Janet bare_decode2(JanetTable *schema, Janet rule, uint8_t *buf,
                          size_t sz, size_t *offset) {

  switch (janet_type(rule)) {
  case JANET_TUPLE: {

    const Janet *t = janet_unwrap_tuple(rule);
    size_t tlen = janet_tuple_length(t);

    if (tlen < 1)
      janet_panic("tuple rule must not be empty");

    if (janet_symeq(t[0], "struct")) {

      if (tlen % 2 != 1)
        janet_panic("struct rule must have an even number of items");

      JanetTable *s = janet_table(tlen);
      for (size_t i = 1; i < tlen; i += 2) {
        Janet key = t[i];
        Janet subrule = t[i + 1];
        Janet value = bare_decode2(schema, subrule, buf, sz, offset);
        janet_table_put(s, key, value);
      }
      return janet_wrap_table(s);

    } else if (janet_symeq(t[0], "map")) {

      if (tlen % 3)
        janet_panic("map rule must have 3 items");

      Janet key_rule = t[1];
      Janet val_rule = t[2];

      int32_t len = decode_size_varuint(buf, sz, offset);

      JanetTable *s = janet_table(len);
      for (int32_t i = 0; i < len; i++) {
        Janet key = bare_decode2(schema, key_rule, buf, sz, offset);
        Janet value = bare_decode2(schema, val_rule, buf, sz, offset);
        janet_table_put(s, key, value);
      }
      return janet_wrap_table(s);

    } else if (janet_symeq(t[0], "optional")) {

      if (tlen != 2)
        janet_panic("optional rule must have 2 items");

      if (sz < *offset + 1)
        unexpected_end_of_buffer_panic();

      int some = buf[*offset];
      *offset += 1;

      if (some) {
        return bare_decode2(schema, t[1], buf, sz, offset);
      } else {
        return janet_wrap_nil();
      }

    } else if (janet_symeq(t[0], "array")) {

      if (tlen == 2) {

        int32_t len = decode_size_varuint(buf, sz, offset);

        JanetArray *array = janet_array(len);
        for (int32_t i = 0; i < len; i++) {
          janet_array_push(array, bare_decode2(schema, t[1], buf, sz, offset));
        }
        return janet_wrap_array(array);

      } else if (tlen == 3) {

        if (!janet_checkint(t[2]))
          janet_panic("expected an integer for array length");

        ssize_t len = janet_unwrap_integer(t[2]);

        JanetArray *array = janet_array(len);
        for (ssize_t i = 0; i < len; i++) {
          janet_array_push(array, bare_decode2(schema, t[1], buf, sz, offset));
        }

        return janet_wrap_array(array);

      } else {
        janet_panicf("array rule must contain 2 or 3 items");
      }

    } else {

      janet_panicf("unknown decoding rule: %v", t[0]);
    }
  }
  case JANET_SYMBOL:
    if (janet_symeq(rule, "void")) {

      return janet_ckeywordv("void");

    } else if (janet_symeq(rule, "u8")) {

      if (sz < *offset + 1)
        unexpected_end_of_buffer_panic();
      Janet v = janet_wrap_number(buf[*offset]);
      *offset += 1;
      return v;

    } else if (janet_symeq(rule, "uint/u64")) {

      uint64_t v;
      if (!decode_varuint(buf, sz, offset, &v))
        could_not_decode_panic();
      return janet_wrap_u64(v);

    } else if (janet_symeq(rule, "uint")) {

      uint64_t v;
      if (!decode_varuint(buf, sz, offset, &v))
        could_not_decode_panic();
      if (v != (uint64_t)(double)v)
        janet_panic("BARE variable int too large for janet number");
      return janet_wrap_number(v);

    } else if (janet_symeq(rule, "data")) {

      int32_t len = decode_size_varuint(buf, sz, offset);
      if (*offset + len > sz)
        unexpected_end_of_buffer_panic();
      JanetBuffer *data = janet_buffer(len);
      janet_buffer_push_bytes(data, buf + *offset, len);
      *offset += len;
      return janet_wrap_buffer(data);

    } else if (janet_symeq(rule, "string")) {

      int32_t len = decode_size_varuint(buf, sz, offset);
      if (*offset + len > sz)
        unexpected_end_of_buffer_panic();
      Janet s = janet_stringv(buf + *offset, len);
      *offset += len;
      return s;
    } else if (janet_symeq(rule, "keyword")) {

      int32_t len = decode_size_varuint(buf, sz, offset);
      if (*offset + len > sz)
        unexpected_end_of_buffer_panic();
      Janet s = janet_keywordv(buf + *offset, len);
      *offset += len;
      return s;

    } else if (janet_symeq(rule, "symbol")) {

      int32_t len = decode_size_varuint(buf, sz, offset);
      if (*offset + len > sz)
        unexpected_end_of_buffer_panic();
      Janet s = janet_symbolv(buf + *offset, len);
      *offset += len;
      return s;

    } else {
      janet_panicf("unsupported rule - %v", rule);
    }
  case JANET_KEYWORD:

    rule = janet_table_get(schema, rule);
    return bare_decode2(schema, rule, buf, sz, offset);

  default:
    janet_panicf("unsupported rule - %v", rule);
  }
}

static Janet bare_decode(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 3);
  JanetTable *schema = janet_gettable(argv, 0);
  Janet rule = argv[1];
  JanetBuffer *buf = janet_getbuffer(argv, 2);
  size_t offset = 0;
  return bare_decode2(schema, rule, buf->data, (size_t)buf->count, &offset);
}

static void bare_encode2(JanetTable *schema, Janet rule, Janet value,
                         JanetBuffer *buf) {

  switch (janet_type(rule)) {
  case JANET_TUPLE: {

    const Janet *t = janet_unwrap_tuple(rule);
    size_t tlen = janet_tuple_length(t);

    if (tlen < 1)
      janet_panic("tuple rule must not be empty");

    if (janet_symeq(t[0], "struct")) {

      if (tlen % 2 != 1)
        janet_panic("struct rule must have an even number of items");

      JanetDictView view = janet_getdictionary(&value, 0);

      for (size_t i = 1; i < tlen; i += 2) {
        Janet key = t[i];
        Janet subrule = t[i + 1];
        Janet value = janet_dictionary_get(view.kvs, view.cap, key);
        bare_encode2(schema, subrule, value, buf);
      }

    } else if (janet_symeq(t[0], "map")) {

      if (tlen % 3)
        janet_panic("map rule must have 3 items");

      JanetDictView view = janet_getdictionary(&value, 0);
      put_varuint(buf, (uint64_t)view.len);

      const JanetKV *kv = NULL;
      while ((kv = janet_dictionary_next(view.kvs, view.cap, kv))) {
        bare_encode2(schema, t[1], kv->key, buf);
        bare_encode2(schema, t[2], kv->value, buf);
      }

    } else if (janet_symeq(t[0], "optional")) {

      if (tlen != 2)
        janet_panic("optional rule must have 2 items");

      if
        janet_checktype(value, JANET_NIL) { janet_buffer_push_u8(buf, 0); }
      else {
        janet_buffer_push_u8(buf, 1);
        bare_encode2(schema, t[1], value, buf);
      }

    } else if (janet_symeq(t[0], "array")) {

      if (tlen == 2) {

        JanetView view = janet_getindexed(&value, 0);
        put_varuint(buf, (uint64_t)view.len);
        for (int32_t i = 0; i < view.len; i++) {
          bare_encode2(schema, t[1], view.items[i], buf);
        }

      } else if (tlen == 3) {

        JanetView view = janet_getindexed(&value, 0);
        int32_t expected_len = janet_getinteger(&t[2], 0);
        if (view.len != expected_len) {
          janet_panicf(
              "array length does not match expected length for rule - %j",
              rule);
        }
        for (int32_t i = 0; i < expected_len; i++) {
          bare_encode2(schema, t[1], view.items[i], buf);
        }

      } else {
        janet_panicf("array rule must contain 2 or 3 items");
      }

    } else {

      janet_panicf("unknown decoding rule: %v", t[0]);
    }

    break;
  }
  case JANET_SYMBOL:
    if (janet_symeq(rule, "void")) {

      /* nothing */

    } else if (janet_symeq(rule, "u8")) {

      uint64_t x = janet_to_uint64(value);
      janet_buffer_push_u8(buf, (uint8_t)x);

    } else if (janet_symeq(rule, "uint/u64")) {

      uint64_t x = janet_to_uint64(value);
      put_varuint(buf, x);

    } else if (janet_symeq(rule, "uint")) {

      uint64_t x = janet_to_uint64(value);
      put_varuint(buf, x);

    } else if (janet_symeq(rule, "data") || janet_symeq(rule, "string") ||
               janet_symeq(rule, "keyword") || janet_symeq(rule, "symbol")) {

      JanetByteView bv = janet_getbytes(&value, 0);
      put_varuint(buf, (uint64_t)bv.len);
      janet_buffer_push_bytes(buf, bv.bytes, bv.len);

    } else {
      janet_panicf("unsupported rule - %v", rule);
    }

    break;
  case JANET_KEYWORD:

    rule = janet_table_get(schema, rule);
    return bare_encode2(schema, rule, value, buf);

  default:
    janet_panicf("unsupported rule - %v", rule);
  }
}

static Janet bare_encode(int32_t argc, Janet *argv) {
  janet_arity(argc, 3, 4);
  JanetTable *schema = janet_gettable(argv, 0);
  Janet rule = argv[1];
  Janet value = argv[2];
  JanetBuffer *buf = (argc == 4 ? janet_getbuffer(argv, 3) : janet_buffer(32));
  bare_encode2(schema, rule, value, buf);
  return janet_wrap_buffer(buf);
}

static const JanetReg cfuns[] = {{"decode", bare_decode,
                                  "(bare/decode schema buffer rule)\n\n"
                                  "Interpret a buffer as a bare message."},
                                 {"encode", bare_encode,
                                  "(bare/encode schema buffer rule value)\n\n"
                                  "Interpret a buffer as a bare message."},
                                 {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) { janet_cfuns(env, "bare", cfuns); }