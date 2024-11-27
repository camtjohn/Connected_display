import csv

temp_array = []
resistance_array = []

with open('lookup_finer.csv') as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    line_count = 0
    for row in csv_reader:
        temp_array.append(row[0])
        resistance_array.append(row[1])
        line_count += 1

    print("Number of elements: ", line_count)

print("")

for elem in temp_array:
    print(elem + ", ", end="")

print("")

for elem in resistance_array:
    print(elem + ", ", end="")

print("")
