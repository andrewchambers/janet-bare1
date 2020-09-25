# janet-bare

WIP (BARE)[https://baremessages.org/] encoder/decoder for janet.

A lot is missing, and the implementation is not optimized, but the concept is there.

# Usage

## Decoding

```
(import bare1 :as bare)

(def schema '@{
  :customer
  (struct :name string
          :email (optional string)
          :metadata (map string string))
})

(def buf @"\x06andrew\x00\x00")

(bare/decode schema :customer buf)
@{:name "andrew" :metadata @{}}
```