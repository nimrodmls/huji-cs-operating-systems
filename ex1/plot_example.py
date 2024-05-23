# OS 24 EX1
import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv(r"C:\Users\nimro\out.csv")
data = data.to_numpy()

kb = 1024 # 1 KiB
mb = 1024 * kb # 1 MiB

l1_size = 192 * kb # 192 KiB
l2_size = 5 * mb # 5 MB
l3_size = 8 * mb # 8 MB

word_size = 8 # 64-bit system
page_size = 4096
pagetable_evic_thres = l3_size * 0.5 * (page_size / word_size)

plt.plot(data[:, 0], data[:, 1], label="Random access")
plt.plot(data[:, 0], data[:, 2], label="Sequential access")
plt.xscale('log')
plt.yscale('log')
plt.axvline(x=l1_size, label="L1 (192 KiB)", c='r')
plt.axvline(x=l2_size, label="L2 (5 MB)", c='g')
plt.axvline(x=l3_size, label="L3 (8 MB)", c='brown')
plt.axvline(x=pagetable_evic_thres, label="PT Evic. Thres.", c='purple')
plt.legend()
plt.title("Latency as a function of array size")
plt.ylabel("Latency (ns log scale)")
plt.xlabel("Bytes allocated (log scale)")
plt.show()
