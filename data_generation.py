import random
import string

TOTAL_RECORDS = 60000

with open("btree_test_load.csv", "w") as f:
    for i in range(1, TOTAL_RECORDS + 1):
        if (random.random() < 0.8):
            # Generate a random payload between 16 and 64 bytes
            length = random.randint(16, 64)
            payload = ''.join(random.choices(string.ascii_letters + string.digits + "_-", k=length))

            # Write Key,Payload
            f.write(f"{i},{payload}\n")
        else:
            length = random.randint(2000, 10000)
            payload = ''.join(random.choices(string.ascii_letters + string.digits + "_-", k=length))

            # Write Key,Payload
            f.write(f"{i},{payload}\n")

print(f"Successfully generated {TOTAL_RECORDS} records.")
