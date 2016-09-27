# Internet of Satellites Protocol

## Description

C API including [OpenFEC](http://openfec.org/) API. Uses Reed Solomon with m=8 to encode a symbol of a given size (typically unlimited) to smaller symbols in order to send it towards a physical layer.


## Compilation

No additional libraries are required
```bash
mkdir build
cd build
cmake ..
make
```

## Execution

At bin folder:

From one terminal (always first receiver, it starts a unix socket server):
`./receiver`

From other terminal:
`./sender`

sender will send symbols towards the receiver, wiht a 20% of PER and random ID.