import random

N = 31
size = 20 * 14
categories = ['ICE', 'FIRE', 'ICE_GOLD', 'MAGICIAN']


print(f'static const uint64_t ZOBRIST_VALUES[][{size}] = {{')

for cat in categories:
    nums = [hex(random.getrandbits(N)) for _ in range(size)]
    joined = ', '.join(nums)
    print(f'    /* {cat} */ {{{joined}}},')

print('};')
