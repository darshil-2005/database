import random
import string

TOTAL_RECORDS = 60000

keys = [i+1 for i in range(0, TOTAL_RECORDS)]
random.shuffle(keys)
print(keys)

with open("btree_test_load_jumbled.csv", "w") as f:
    for i in range(0, TOTAL_RECORDS):
        if (random.random() < 0.8):
            # Generate a random payload between 16 and 64 bytes
            length = random.randint(16, 64)
            payload = ''.join(random.choices(string.ascii_letters + string.digits + "_-", k=length))

            # Write Key,Payload
            f.write(f"{keys[i]},{payload}\n")
        else:
            length = random.randint(1200, 10000)
            payload = ''.join(random.choices(string.ascii_letters + string.digits + "_-", k=length))

            # Write Key,Payload
            f.write(f"{keys[i]},{payload}\n")

print(f"Successfully generated {TOTAL_RECORDS} records.")
