import os
import re
import sys

def check_files(directory):
    # Check if the directory exists
    if not os.path.isdir(directory):
        print(f"Error: {directory} is not a valid directory.")
        return
    
    # Iterate over files in the directory
    for filename in os.listdir(directory):
        filepath = os.path.join(directory, filename)
        
        # Check if the filepath is a file
        if os.path.isfile(filepath):
            # Read the content of the file
            with open(filepath, 'r') as file:
                content = file.read()
                # Check if the content contains "s SATISFIABLE"
                if "s SATISFIABLE" in content:
                    print(f"File {filename} contains 's SATISFIABLE'.")

                    # Extract the hash file from the log file name using regex
                    match = re.match(r"log_(\S+)\.txt", filename)
                    if match:
                        hash_file = match.group(1)
                        # Generate the input file path
                        input_file = f"../../INPUTS/decompressed/{hash_file}.cnf"
                        # Execute the SAT solver with input file and log file paths
                        os.system(f"./SAT {input_file} {filepath}")

# Check if directory path is provided as argument
if len(sys.argv) != 2:
    print("Usage: python script.py <directory_path>")
    sys.exit(1)

directory_path = sys.argv[1]
check_files(directory_path)
