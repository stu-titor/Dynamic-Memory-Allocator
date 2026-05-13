#! /usr/bin/env python3
import subprocess
import statistics
import os
import argparse
import math
import sys

try:
    from tabulate import tabulate
except:
    print("⚠️ Tabulate module is not found. Installing tabulate module. ⚠️")
    subprocess.run(['python3', "-m", "pip", "install", 'tabulate', '--user'])
finally:
    from tabulate import tabulate
    print("Tabulate import successful. ✅")

parser = argparse.ArgumentParser()
parser.add_argument("-w", "--week", type=int, help="Week tests to run.")
args = parser.parse_args()

# depends on what range we are targetting
utilization_target = 72.50
performance_target = 8500
timeout_limit = 240
total_deferred_coalesces = 0
total_immediate_coalesces = 0
immediate_coalesce_traces = []

def heap_runner_check():
    """
    Runs the heap_runner executable and retrieves its exit code.
    Returns the result of (5 - score) from heap_runner's main or -1 if it fails (manual grading needed).
    """
    heap_runner = subprocess.run(["./heap_runner"], universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout_limit)
    if heap_runner.returncode == -1:
        return 0
    
    if 0 <= heap_runner.returncode <= 5:
        score = 5 - heap_runner.returncode
        return score / 5.0
    
    return 0

def get_num_ops(trace_file):
    f = open(trace_file, "r")
    num_ops = int(f.readlines()[1])
    return num_ops

def bump_test():
    bump_correctness = subprocess.run(['./bump_test'], universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) 
    return -1 if bump_correctness.returncode != 0 else 0

def runner_coalesce_check():
    runner_coalesce_val = subprocess.run(['./runner','-d'], universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) 
    if 'PASSED' in runner_coalesce_val.stdout:
        return 1
    return 0

def performance_check(trace_file):
    # number of times it runs the trace_file
    N = 30
    total_time = 0
    num_ops = get_num_ops(trace_file)
    for i in range(0, N):
        performance = subprocess.run(["./performance", trace_file], universal_newlines=True, stdout=subprocess.PIPE, timeout=timeout_limit)
        if 'Success' not in performance.stdout:
            return -1
        total_time += int(performance.stdout.split()[1])
    return (num_ops / (total_time // N)) * 1000

def utilization_check(trace_file):
    utilization = subprocess.run(["./runner", '-ru', trace_file], universal_newlines=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE, timeout=timeout_limit)
    if utilization.returncode != 0:
        return -1
    return_array = utilization.stdout.split('\n')
    # print(return_array)
    utilization_percentage = float(return_array[4].split()[3])
    return utilization_percentage

def correctness_check(trace_file, week):
    global total_deferred_coalesces
    global total_immediate_coalesces
    global immediate_coalesce_traces

    if week == 2:
        correctness = subprocess.run(["./runner", '-r', trace_file], universal_newlines=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE, timeout=timeout_limit)
        for line in correctness.stdout.split('\n'):
            if "COALESCE_STATS" in line:
                try:
                    parts = line.split()
                    deferred_count = int(parts[2])
                    immediate_count = int(parts[4])

                    total_deferred_coalesces += deferred_count
                    total_immediate_coalesces += immediate_count
                    if immediate_count > 0:
                        immediate_coalesce_traces.append(trace_file)
                except (IndexError, ValueError):
                    pass
    else:
        correctness = subprocess.run(["./runner", '-r', trace_file], universal_newlines=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE, timeout=timeout_limit)
    # possibly not coalesed
    # print(correctness.returncode)

    if correctness.returncode == 1:
        return 'umalloc package passed correctness check.' in correctness.stdout
    # definitely failed
    elif correctness.returncode == 3:
        print('Did not use all bins')
        return False
    elif correctness.returncode == 4:
        print('uinit failed')
        return False
    elif correctness.returncode == 5:
        print('bin count below required count')
        return False
    else:
        # print(coalesced)
        return 'umalloc package passed correctness check.' in correctness.stdout

trace_correctness = []
trace_utilization = []
trace_performance = []
table = []

def run_trace(trace_file, week):
    global trace_correctness
    global trace_utilization
    global trace_performance
    global table
    passed = correctness_check(trace_file, week)
    trace_correctness += [passed]
    util = -1
    perf = -1
    if passed:
        if not 'oom' in trace_file:
            util = utilization_check(trace_file)
            trace_utilization += [util]
            perf = performance_check(trace_file)
            trace_performance += [perf]
        else:
            perf = 0
            util = 0
    correct = 'Yes' if passed else 'No' 
    table += [[trace_file, correct, util, perf]]


def run_tests(week=2):
    global trace_correctness
    global trace_utilization
    global trace_performance
    global table
    global total_deferred_coalesces, total_immediate_coalesces
    global immediate_coalesce_traces

    trace_correctness = []
    trace_utilization = []
    trace_performance = []
    table = []
    total_deferred_coalesces = 0
    total_immediate_coalesces = 0
    immediate_coalesce_traces = []
    
    effective_week = args.week if args.week else 2

    os.system("make clean; make all")
    for file in os.listdir("./traces"):
        if effective_week == 1:
            #run only short testcases
            if file == 'week1':
                for trace in os.listdir(os.path.join("./traces", file)):
                    if trace.endswith(".rep"):
                        print(trace)
                        run_trace(os.path.join("./traces", file, trace), effective_week)
        else:
            # run the long testcases
            if file == "week2":
                for folder in sorted(os.listdir(os.path.join("./traces", file))):
                    if folder != 'README':
                        for trace in os.listdir(os.path.join("./traces", file, folder)):
                            print(trace)
                            run_trace(os.path.join("./traces", file, folder, trace), effective_week)

    utilization_average = sum(trace_utilization) / (1 if len(trace_utilization) == 0 else len(trace_utilization))
    performance_average = sum(trace_performance) / (1 if len(trace_performance) == 0 else len(trace_performance))
    correctness_average = sum(trace_correctness) / (1 if len(trace_correctness) == 0 else len(trace_correctness))
    table += [["Average", "{:.2f}".format(correctness_average * 100), "{:.2f}".format(utilization_average), "{:.2f}".format(performance_average)]]
    print (tabulate(table, headers=["Trace", "Passed", "Utilization", "Performance (Operations per millisecond)"]))

    # Score calculation
    utilization_score = 55 * (utilization_average / utilization_target)
    if utilization_score < 35 and effective_week == 2:
        print(f'Your utilization was {utilization_score:.2f}, which is below the minimum of 35 required to receive utilization credit.')
        utilization_score = 0

    performance_score = 20 * (performance_average / performance_target)

    correctness_average = sum(trace_correctness) / len(trace_correctness)
    correctness_score = 15 * correctness_average
    scale_factor = 1
    if correctness_average < 1.0:
        scale_factor = 0

    # do bump test
    bump_correctness = bump_test()
    if bump_correctness == -1:
        print("🚨 Did not pass bump allocation test and/or implicit free list test 🚨")
        correctness_score = 0
        scale_factor = 0
    else:
        print("No bump allocation and/or implicit free list detected. ✅\nPlease note that we will do manual verification after the assignment is turned in.⚠️")

    coalescing_penalty = False
    if effective_week == 2:
        if total_immediate_coalesces > 0:
            print(f"🚨 Immediate coalescing detected ({total_immediate_coalesces} events). 🚨")
            print("TRACES WITH IMMEDIATE COALESCING:")
            for t in immediate_coalesce_traces:
                print(f" - {t}")
            coalescing_penalty = True
        elif total_deferred_coalesces == 0 or runner_coalesce_check() == 0:
            print("🚨 No deferred coalescing detected across any traces. 🚨")
            print("TRACES CHECKED (None coalesced)!")
            coalescing_penalty = True
        else:
            print(f"Deferred coalescing verified ({total_deferred_coalesces} events). ✅")

    # max out scores so we can't go over
    performance_score = min(performance_score, 25)
    utilization_score = min(utilization_score, 60)
    # student ran the code
    if effective_week == 1:
        heap_runner_score = heap_runner_check()
        correctness_score = 0 if correctness_average < 1.0 else 3
        final_score = heap_runner_score + correctness_score
        print("Your heap_runner score is: " + str(heap_runner_score))
        print ("Note that your utilization & throughput do NOT affect your checkpoint grade for week 1.")
        print ("Total Score " + str(final_score))
        return heap_runner_score, correctness_score
    else:
        final_score = math.ceil(correctness_score + ((performance_score + utilization_score) * scale_factor))

        if coalescing_penalty:
            final_score = correctness_score
            print("Coalescing requirements not met.")
        print (f"Correct: {correctness_score}/15, perf: {performance_score}/20, util: {utilization_score}/55")
        print ("Total Score " + str(final_score) + " / 90")
        print ("The other ten points come from the writeup after the assignment is turned in.")
        return final_score

if __name__ == '__main__':
    run_tests()
        
