'''
    Simulate the behavior of thread pool 2 in terms of thread creation
'''
import plotext as plt
import argparse

TOTAL_TICKS=32

jobs = [0 for _ in range(TOTAL_TICKS)]
total_threads = [0 for _ in range(TOTAL_TICKS)]
send_blocked_threads = [0 for _ in range(TOTAL_TICKS)]
recv_blocked_threads = [0 for _ in range(TOTAL_TICKS)]

parser = argparse.ArgumentParser(description="Threadpool2 parameters.")

# Define parameters with defaults
parser.add_argument("--min", type=int, default=2, help="Minimum threads (default: 2)")
parser.add_argument("--max", type=int, default=6, help="Maximum threads (default: 6)")
parser.add_argument("--hysteresis", type=int, default=2, help="Hysteresis value (default: 2)")
parser.add_argument("--inc", type=int, default=1, help="Increment/Step size (default: 1)")

args = parser.parse_args()

MIN_THREADS=args.min
MAX_THREADS=args.max
HYSTERESE=args.hysteresis
THREAD_INC=args.inc


''' input function '''
def populate_jobs(jobs):
    idx = 0
    while (idx < 4):
        jobs[idx] = MIN_THREADS - 1
        idx += 1

    while (idx < 6):
        jobs[idx] = MIN_THREADS
        idx += 1

    while (idx < 12):
        jobs[idx] = MAX_THREADS + 2
        idx += 1
    while (idx < 16):
        jobs[idx] = idx & 1
        idx += 1
    while (idx < 20):
        jobs[idx] = (idx & 1) * (MIN_THREADS + 1)
        idx += 1
    while (idx < 24):
        jobs[idx] = (idx & 1) * HYSTERESE
        idx += 1


''' compute receive blocked threads'''
def rb(tick):
    if jobs[tick] < total_threads[tick - 1]:
        return total_threads[tick - 1] - jobs[tick]
    else:
        return 0
    
''' compute send blocked threads '''
def sb(tick):
    if rb(tick) > 0:
        return 0
    else:
        return jobs[tick] - total_threads[tick - 1]
    
''' compute total number of thread pool threads'''
def total(tick):
    if total_threads[tick - 1] < MIN_THREADS:
        return MIN_THREADS
    
    if sb(tick) > 0:
        return min(MAX_THREADS, total_threads[tick - 1] + THREAD_INC)
    
    if rb(tick) >= HYSTERESE:
        return max(MIN_THREADS, total_threads[tick - 1] - HYSTERESE)
    
    return total_threads[tick - 1]

populate_jobs(jobs)

ticks = [0 for _ in range(2*TOTAL_TICKS)]

''' main loop '''
for tick in range(1, TOTAL_TICKS):
    recv_blocked_threads[tick] = rb(tick)
    send_blocked_threads[tick] = sb(tick)
    total_threads[tick] = total(tick)

# insert extra points at corners
# s stands for "display"
djobs =[]
dtt = []
drbt = []
dsbt = []
ticks = []

for tick in range(1, TOTAL_TICKS):
    if total_threads[tick] != total_threads[tick - 1] or \
        recv_blocked_threads[tick] != recv_blocked_threads[tick - 1] or \
            send_blocked_threads[tick] != send_blocked_threads[tick - 1] or \
            jobs[tick] != jobs[tick - 1]:
        ticks.append(tick)
        djobs.append(jobs[tick - 1])
        dtt.append(total_threads[tick - 1])
        dsbt.append(send_blocked_threads[tick - 1])
        drbt.append(recv_blocked_threads[tick - 1])

    ticks.append(tick)
    djobs.append(jobs[tick])
    dtt.append(total_threads[tick])
    dsbt.append(send_blocked_threads[tick])
    drbt.append(recv_blocked_threads[tick])
    
plt.subplots(4, 1)

plt.subplot(1,1)
plt.title("Jobs")
plt.plot(ticks, djobs)

plt.subplot(2,1)
plt.title("pool threads")
plt.plot(ticks, dtt)

plt.subplot(3,1)
plt.title("recv blocked threads")
plt.plot(ticks, drbt)

plt.subplot(4,1)
plt.title("send blocked threads")
plt.plot(ticks, dsbt)

plt.show()