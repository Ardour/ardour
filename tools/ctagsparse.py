#!/usr/bin/python
import csv

# ctags -o /tmp/atag.csv --fields=+afiKkmnsSzt libs/ardour/ardour/export_format_specification.h 

f = open('/tmp/atag.csv', 'rb')
reader = csv.reader(f, delimiter='\t')
for row in reader:
    if len(row) > 7:
        print row[7]
