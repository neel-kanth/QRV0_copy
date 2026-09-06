# Cherry

Cherry provides a simple and asynchronous API to implement UWB use-cases.

It runs on Linux and Zephyr hosts, and supports the following session types:

- FiRa TWR
- FiRa DL-TDoA
- Radar

This package is composed of the following:

- `cherry`: the core library that implements the API and session types;
- `uwbs_config`: a configuration library that provides an API to load an UWBS
  calibration file on a QM35 device;
- `examples/cherry`: a set of examples that demonstrate how to use the Cherry
  library;
- `examples/uci_bridge`: a tool that allows to run the Cherry examples on a
  remote PC, and communicate with a QM35 device attached to a Linux or Zephyr
  host, over a serial port or USB-C cable.

## Building the Cherry library and examples

The Cherry library and examples can be built using the `cmake` build system, or
using the `Recipes` helper makefile:

```bash
make -f Recipes host.cmake host.all
```

Please see the *How to build Cherry* chapter of the
`cherry-api-and-integration-guide.pdf` document for prerequisites,
cross-compilation instructions, and additional information.

## Running the examples

The examples can either be run directly from a Linux or Zephyr host equipped
with a QM35 HDK interface board, or from a remote PC connected to the host
running the `uci-bridge` tool.

Please see the `uci-bridge-and-certification-guide.pdf` document for additional
information.
