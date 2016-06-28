#!/usr/bin/perl -w

# Finds the average flow completion time vs. the flow size.

$inFile = $ARGV[0];
$outFile = $ARGV[1];

# Initialize the arrays.
for ($j = 0; $j < 10000000; $j++) {
  $sumDuration[$j] = 0;
  $avgDuration[$j] = 0;
  $maxDuration[$j] = 0;
  $numFlows[$j] = 0;
}

# Open the input and output files.
open(fileOut, ">$outFile") or die("Can't open $outFile for writing: $!");
open(fileIn, "$inFile") or die("Can't open $inFile: $!");

$maximum = 0;
$simTime = 65;
while (<fileIn>) {
  chomp;
  @items = split;
  if ($items[1] <= $simTime) {
    if ($items[7] > $maximum) {
      $maximum = $items[7];
    }
    $sumDuration[$items[7]] += $items[9];
    if ($items[9] > $maxDuration[$items[7]]) {
      $maxDuration[$items[7]] = $items[9];
    }
    $numFlows[$items[7]] += 1;
    $avgDuration[$items[7]] = $sumDuration[$items[7]] / $numFlows[$items[7]];
  }
}

for ($j = 1; $j <= $maximum; $j++) {
  if ($avgDuration[$j] != 0) {
    # Output format: flowSize averageFlowDuration
    printf fileOut "$j $avgDuration[$j]\n";
  }
}

close(fileIn);
close(fileOut);
