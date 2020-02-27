import sys
import struct
import ctypes


if len(sys.argv) < 2:
    print(f'Usage: {sys.argv[0]} <.ice>')
    sys.exit(1)


class FloorFileHeader(ctypes.Structure):
    _fields_ = [
        ('sig', ctypes.c_byte * 12),
        ('comment', ctypes.c_ubyte * 108),
        ('version', ctypes.c_int),
        ('floorCount', ctypes.c_int)]


MAP_W = 20
MAP_H = 14
MAP_SIZE = MAP_W * MAP_H
FLOOR_FILE_SIG = b'IceTower\r\n\x1a\0'
assert len(FLOOR_FILE_SIG) == 12

FLOOR_REPR_MAP = {
    0:   (' ', 0),   # space
    1:   ('#', 0),   # wall
    2:   ('%', 1),   # ice
    3:   ('*', 1),   # fire
    4:   ('^', 4),   # ]- arrows
    5:   ('v', 4),   # ]
    6:   ('<', 4),   # ]
    7:   ('>', 4),   # ]
    8:   ('+', 5),   # dispenser
    9:   ('-', 2),   # recycler
    10:  ('$', 3),   # golden ice
    15:  ('!', -1),  # crystal
    255: ('@', -1),  # magician
}


# "c_char"s cannot be used because contents beyond null byte is truncated
# upon reading its bytes representation, but there is no trivial way to
# retrieve the pointer to array itself.
def ctype_array_to_bytes(arr):
    return b''.join([bytes((x,)) for x in arr])


with open(sys.argv[1], 'rb') as f:
    header = FloorFileHeader()

    if f.readinto(header) != ctypes.sizeof(header):
        raise ValueError('Malformed file')

    sig = ctype_array_to_bytes(header.sig)

    if sig != FLOOR_FILE_SIG:
        raise ValueError(f'The signature of file is incorrect: "{sig}"')
    if header.version != 1:
        raise ValueError(f'Unsupported version: "{header.version}"')

    comment = ctype_array_to_bytes(header.comment).rstrip(b'\0')

    print(f'Version: {header.version}')
    print(f'Comment: {comment}')
    print(f'Floor count: {header.floorCount}')

    for idx in range(header.floorCount):
        print(f'---- Floor #{idx+1:3d} ----\n')

        content_ = f.read(MAP_SIZE)
        if len(content_) != MAP_SIZE:
            raise ValueError('Unexpected end of file')

        content = iter(content_)

        diff = 0

        buf = []

        for i in range(MAP_H):
            for j in range(MAP_W):
                c = int(next(content))

                r, d = FLOOR_REPR_MAP.get(c, ('?', -1))
                diff = max(d, diff)

                buf.append(r)
            buf.append('\n')

        if diff >= 5:
            continue

        print(''.join(buf))
        print(f'Difficulty = {diff}')
        print()

    x = f.read()
    if x:
        print(f'Trailing (?): {x}')
