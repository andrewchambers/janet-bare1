# janet-bare1

WIP [BARE](https://baremessages.org) encoder/decoder for janet.

A lot is missing, and the implementation is not optimized, but the concept is there.

# Usage

```
(import bare1 :as bare)
 
(def schema '@{
  
  :customer
    (struct :name string
            :email (optional string)
            :metadata (map string string)
            :subscriptions (array :subscription))

  :subscription
    (struct :product string
            :plan uint)
})


(def customer
  @{:name "andrew"
   :metadata @{}
   :subscriptions @[@{:product "dogfood" :plan 127}]})

(def buf (bare/encode schema :customer customer))

# (pp buf)
# @"\x06andrew\x00\x00\x01\x07dogfood\x7f"

(bare/decode schema :customer buf)
# Decodes back to:
# @{:name "andrew"
#   :metadata @{}
#   :subscriptions @[@{:product "dogfood" :plan 127}]}
      
```

## Janet specific extensions and limitations

- The special rules 'keyword and 'symbol correspond to a BARE string, but are encoded/decoded as a janet keyword/symbol.
- The 'uint rule corresponds to a janet double, you must use 'uint/u64 to explicitly decode to an int/u64 boxed integer.
- Arrays, maps and buffers are limited to 0x7fffffff elements corresponding with janet implementation limits.
