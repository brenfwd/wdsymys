import subprocess
import sys
import time
import psutil

NUM_SAMPLES = 10

def monitor_memory(pid):
    try:
        process = psutil.Process(pid)
        peak_memory = 0
    
        while process.is_running() and process.status() != psutil.STATUS_ZOMBIE:
            try:
                mem_info = process.memory_info()
                peak_memory = max(peak_memory, mem_info.rss)
            except psutil.NoSuchProcess:
                break
        return peak_memory
    except psutil.NoSuchProcess:
        print("prozesse ist schon gestorben")
        return 0

def measure_execution_time(command):
    try:
        avg_time = 0
        avg_mem = 0
        
        stdout = ""
        stderr = ""
        # Start the timer
        for i in range(NUM_SAMPLES):
            start_time = time.perf_counter()
        
            # Run the command
            process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            pid = process.pid
            peak_memory = monitor_memory(pid)
        
            # Stop the timer
            end_time = time.perf_counter()
            stdout, stderr = process.communicate() 
            # Calculate elapsed time
            elapsed_time = end_time - start_time
            avg_time = ((avg_time * i) + elapsed_time) / (i + 1)
            avg_mem = ((avg_mem * i) + peak_memory) / (i + 1)
        
        # Output the program's stdout, stderr, and execution time
        print("Output:\n", stdout)
        print("Error:\n", stderr if stderr else "None")
        print(f"Execution Time: {avg_time:.4f} seconds")
        print(f"Peak memory: {avg_mem / 1024 / 1024:.2f} MB")
        
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python measure_time.py '<command>'")
        sys.exit(1)
    
    command = sys.argv[1]
    measure_execution_time(command)

