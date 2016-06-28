import os
import sys

import matplotlib.pyplot as plt

def make_graph(data, title, save):
  markers = ['*', 'o', 's', '^', 'd']
  fig = plt.figure()
  ax = fig.add_subplot(111)

  count = 0
  for label, x, y in data:
    ms = 20
    if label == 'SPROUT':
      ms = 30
    plt.plot(x, y, markers[count % len(markers)], label = label, ms = ms)
    count += 1
    #ax.annotate('%s' % label, xy=(x, y), textcoords='data')
  ax.set_xscale('log')
  plt.gca().invert_xaxis()
  plt.title(title)
  plt.xlabel('95th percentile signal delay (ms)')
  plt.ylabel('Throughput (Mb/s)')
  plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
  plt.ylim(0)
  print 'Saving: %s' % save
  plt.savefig(save, bbox_inches='tight')

def get_data_by_downlink(link, values):
  rows = []
  for d in values:
    if d['downlink'] == link:
      rows.append(d)
  return rows

if len(sys.argv) != 3:
  print 'Usage: python graph_graphs.py <input.tsv> <output_prefix>'
  sys.exit(1)

infile = sys.argv[1]
output_prefix = sys.argv[2]

headers = None
values = []
links = []
for line in open(infile, 'r'):
  l = line.strip()
  if headers is None:
    headers = line.split('\t')
  else:
    bits = line.split('\t')  
    if len(bits) != len(headers):
      print 'Anger! this tsv file is malformatted'
    entry = {}
    for i in range(0, len(bits)):
      entry[headers[i]] = bits[i] 
      if headers[i] == 'downlink' and not bits[i] in links:
        links.append(bits[i])
    values.append(entry)

# now make graphs for each of the up/down link pairs
print 'Making graphs for each link: '
print links

for link in links:
  rows = get_data_by_downlink(link, values)
  # now make these rows into (labels, xs, ys) tuples
  data = [(r['protocol'], r['delay'], r['throughput']) for r in rows]
  title = link.split('.')[0].strip()
  make_graph(data, 'Throughput vs. signal delay for ' + title, output_prefix + '_' + title + '.png')


