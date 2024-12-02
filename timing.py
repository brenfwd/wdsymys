import subprocess
import sys
import time

def measure_execution_time(command):
    try:
        # Start the timer
        start_time = time.perf_counter()
        
        # Run the command
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        
        # Stop the timer
        end_time = time.perf_counter()
        
        # Calculate elapsed time
        elapsed_time = end_time - start_time
        
        # Output the program's stdout, stderr, and execution time
        print("Output:\n", result.stdout)
        print("Error:\n", result.stderr if result.stderr else "None")
        print(f"Execution Time: {elapsed_time:.4f} seconds")
        
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python measure_time.py '<command>'")
        sys.exit(1)
    
    command = sys.argv[1]
    measure_execution_time(command)

