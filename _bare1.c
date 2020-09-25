#include <janet.h>
#include <sys/types.h>

static int decode_varuint(uint8_t *buf, size_t sz, size_t *offset, uint64_t *out) {
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
      v |= ((uint64_t)(b&0x7f)) << s;
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

static void varint_exceeded_limits_panic() {
  janet_panic("BARE variable int too large for janet number");
}

static Janet bare_decode2(JanetTable *schema, Janet rule, uint8_t *buf, size_t sz, size_t *offset) {
  
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
          Janet subrule = t[i+1];
          Janet value = bare_decode2(schema, subrule, buf, sz, offset);
          janet_table_put(s, key, value);
        }
        return janet_wrap_table(s);

      } else if (janet_symeq(t[0], "map")) {

        if (tlen % 3)
          janet_panic("map rule must have 3 items");
        
        Janet key_rule = t[1];
        Janet val_rule = t[2];

        uint64_t len;
        if (!decode_varuint(buf, sz, offset, &len))
          could_not_decode_panic();
        if (len > 0x7fffffff)
          message_too_large_panic();

        JanetTable *s = janet_table(len);
        for (size_t i = 0; i < len; i ++) {
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
         
          uint64_t len;
          if (!decode_varuint(buf, sz, offset, &len))
            could_not_decode_panic();
          if (len > 0x7fffffff)
            message_too_large_panic();
          
          JanetArray *array = janet_array(len);
          for (size_t i = 0; i < len; i++) {
            janet_array_push(array, bare_decode2(schema, t[1], buf, sz, offset));
          }
          return janet_wrap_array(array);

        } else if (tlen == 3) {
          
          if (!janet_checkint(t[1]))
            janet_panic("expected an integer for array length");

          ssize_t len = janet_unwrap_integer(t[1]);

          JanetArray *array = janet_array(len);
          for (ssize_t i = 0; i < len; i++) {
            janet_array_push(array, bare_decode2(schema, t[2], buf, sz, offset));
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
            varint_exceeded_limits_panic();
          return janet_wrap_number(v);

      } else if (janet_symeq(rule, "data")) {
          
        uint64_t len;
        if (!decode_varuint(buf, sz, offset, &len))
          could_not_decode_panic();
        if (len > 0x7fffffff)
          message_too_large_panic();
        if (*offset+len > sz)
          unexpected_end_of_buffer_panic();
        JanetBuffer *data = janet_buffer(len);
        janet_buffer_push_bytes(data, buf+*offset, len);
        *offset += len;
        return janet_wrap_buffer(data);

      } else if (janet_symeq(rule, "string")) {
          
        uint64_t len;
        if (!decode_varuint(buf, sz, offset, &len))
          could_not_decode_panic();
        if (len > 0x7fffffff)
          message_too_large_panic();
        if (*offset+len > sz)
          unexpected_end_of_buffer_panic();
        Janet s = janet_stringv(buf + *offset, len);
        *offset += len;
        return s;
      } else if (janet_symeq(rule, "keyword")) {
          
        uint64_t len;
        if (!decode_varuint(buf, sz, offset, &len))
          could_not_decode_panic();
        if (len > 0x7fffffff)
          message_too_large_panic();
        if (*offset+len > sz)
          unexpected_end_of_buffer_panic();
        Janet s = janet_keywordv(buf + *offset, len);
        *offset += len;
        return s;

      } else if (janet_symeq(rule, "symbol")) {
          
        uint64_t len;
        if (!decode_varuint(buf, sz, offset, &len))
          could_not_decode_panic();
        if (len > 0x7fffffff)
          message_too_large_panic();
        if (*offset+len > sz)
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

static const JanetReg cfuns[] = {
    {"decode", bare_decode,
     "(bare/decode schema buffer rule)\n\n"
     "Interpret a buffer as a bare message."},
    {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) { janet_cfuns(env, "bare", cfuns); }