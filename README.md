# qits
Qbane's Ice Tower Solver

## [Project scratchpad](https://hackmd.io/@q/ice-tower)

# Text representation of levels

Each floor is stored as a text file. Only the first 14 lines & first 20 bytes of each line would be read. Note that the format is subject to change. If you have a binary `.ice` format file, use `scripts/dump.py` to dump out all floors. Refer to `levels/` for examples.

# To build

You may need Python 3.6+.

```bash
make zobrist_values
make
```

# Usage

Trivial. Feed a level from stdin, and a solution is printed to stdout if found.
