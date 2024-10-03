import os
import re
import csv
import sys

# Specify the folder containing log files
if len(sys.argv) < 2:
    print("Usage: python script_name.py folder_path")
    quit()

folder_path = sys.argv[1]

# Define regular expressions to capture best SBVA IDs
best_ls_pattern = re.compile(r"Best SBVA according to LocalSearch is : (\d+)")
least_clauses_pattern = re.compile(r"Best SBVA according to nbClausesDeleted is : (\d+)")
winner_pattern = re.compile(r"The winner is (\d+) .+")

# Function to process a single log file
def process_log_file(log_file_path, best_ls_id, least_clauses_id):
    # Define regular expressions to match relevant log lines
    sbva_pattern = re.compile(r"\[SBVA {}\] varCount: (\d+), realClauseCount: (\d+), adjacencyDeleted: (\d+)".format(best_ls_id))
    sbva_pattern_two = re.compile(r"\[SBVA {}\] varCount: (\d+), realClauseCount: (\d+), adjacencyDeleted: (\d+)".format(least_clauses_id))
    yalsat_pattern = re.compile(r"\[YalSat {}\] Number of remaining unsats (\d+) / (\d+), Number of Flips (\d+)".format(best_ls_id))
    yalsat_pattern_two = re.compile(r"\[YalSat {}\] Number of remaining unsats (\d+) / (\d+), Number of Flips (\d+)".format(least_clauses_id))

    # Initialize variables to store extracted information
    nb_unsat = None
    nb_unsat_two = None
    remaining_clauses = None
    remaining_clauses_two = None
    adjacency_deleted = None
    adjacency_deleted_two = None
    winnerId = None

    # Parse the log file and extract the required information
    with open(log_file_path, 'r') as file:
        for line in file:
            # Match SBVA log lines
            sbva_match = sbva_pattern.search(line)
            if sbva_match:
                remaining_clauses, adjacency_deleted = sbva_match.group(2), sbva_match.group(3)

            sbva_match_two = sbva_pattern_two.search(line)
            if sbva_match_two:
                remaining_clauses_two, adjacency_deleted_two = sbva_match_two.group(2), sbva_match_two.group(3)

            # Match YalSat log lines
            yalsat_match = yalsat_pattern.search(line)
            yalsat_match_two = yalsat_pattern_two.search(line)
            if yalsat_match:
                nb_unsat = yalsat_match.group(1)
            if yalsat_match_two:
                nb_unsat_two = yalsat_match_two.group(1)

            winner_match = winner_pattern.search(line)
            if winner_match:
                winnerId = winner_match.group(1)
            
    return nb_unsat, nb_unsat_two, adjacency_deleted, adjacency_deleted_two, winnerId

# Initialize the CSV file and write the header
with open('output.csv', 'w', newline='') as csvfile:
    fieldnames = ['LogFileName', 'BestLocalSearch', 'BestNbClausesDeleted', 'NbUnsatBestLs', 'NbUnsatBestSBVA', 'AdjacencyDeletedBestLs', 'AdjacencyDeletedBestSBVA', 'WinnerId']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()

    # Iterate over each file in the folder
    for filename in os.listdir(folder_path):
        if filename.endswith('.txt'):  # Ensure we only process text files
            log_file_path = os.path.join(folder_path, filename)

            best_ls_id = None;
            least_clauses_id = None;
            
            # Process log file to capture best LS and least clauses IDs
            with open(log_file_path, 'r') as file:
                for line in file:
                    # Match lines specifying the best SBVA according to LocalSearch
                    best_ls_match = best_ls_pattern.search(line)
                    if best_ls_match:
                        best_ls_id = best_ls_match.group(1)

                    # Match lines specifying the best SBVA according to nbClausesDeleted
                    least_clauses_match = least_clauses_pattern.search(line)
                    if least_clauses_match:
                        least_clauses_id = least_clauses_match.group(1)
                        break
            
            # Process log file again to capture relevant information based on best LS and least clauses IDs
            nb_unsat, nb_unsat_two, adjacency_deleted, adjacency_deleted_two, winnerId = process_log_file(log_file_path, best_ls_id, least_clauses_id)
            print("Ls: "+str(best_ls_id)+", sbva: "+str(least_clauses_id))
            # Write the extracted information to the CSV file
            writer.writerow({'LogFileName': filename, 'BestLocalSearch': best_ls_id, 'BestNbClausesDeleted': least_clauses_id, 'NbUnsatBestLs': nb_unsat, 'NbUnsatBestSBVA': nb_unsat_two, 'AdjacencyDeletedBestLs': adjacency_deleted, 'AdjacencyDeletedBestSBVA': adjacency_deleted_two, 'WinnerId' : winnerId})
