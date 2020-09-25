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
            :metadata (map string string)
            :subscriptions (array :subscription))

  :subscription
    (struct :product string
            :plan uint)
})

(def buf @"\x06andrew\x00\x00\x01\x07dogfood\xff\x00")

(_bare/decode schema :customer buf)

# Decodes to:
 @{:name "andrew"
   :metadata @{}
   :subscriptions @[@{:product "dogfood" :plan 127}]}
      
```

## Encoding

TODO...