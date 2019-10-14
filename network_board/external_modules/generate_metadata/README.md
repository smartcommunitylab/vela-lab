Generate Metadata
===============================

This program takes a binary firmware and some parameters as input and generates the metadata.
This program should be compiled for the machine used during the development, and not for any MCU platform

This program is executed by build_node_ota.sh and you just need to compile it the first time:

```bash
make generate-metadata
```
