# SCOT

Please see the detailed installation and usage instructions in the artifact.

## Building

```
cd SCOT
make
```

## Usage Example

Test linked list (Harris-Michael, Harris' lock-free linked lists). In the example below, 10 is 10 seconds, 16 is the key range and number of elements, 1 is the number of iterations, 50 is the percentage of reads, 25 is the percentage of inserts, 25 is the percentage of deletes, IBR is the reclamation scheme (can also be NR, EBR, HP, HPO, HE, HYALINE), and 4 is the number of threads (up to 384 in the current implementation). Note that HP stands for the optimized version of Hazard Pointers, whereas HPO is the basic (original) version of Hazard Pointers.

```
./SCOT/bench listlf 10 16 1 50 25 25 IBR 4
```

For the linked list with wait-free traversals, run:

```
./SCOT/bench listwf 10 16 1 50 25 25 IBR 4
```

Finally, to test Natarajan-Mittal tree, run:

```
./SCOT/bench tree 10 100000 1 50 25 25 IBR 4
```

## Running tests

If you go to Scripts, you can run a full-blown test with 5 iterations:

```
nohup ./source.sh &
```

You can also run a faster (1 iteration) test:

```
nohup ./source_fast.sh &
```

Finally, you can run the faster (1 iteration) test without the NR baseline (when your RAM size is limited):

```
nohup ./source_lightweight.sh &
```


## Data

When running the experiment scripts, they will put output files into the Data directory. You can then generate charts that are presented in the paper:

```
python3 generate_charts.py paper
```

For space efficiency, paper charts do not include a few (unimportant) data points. To generate charts for the entire data set, run:

```
python3 generate_charts.py
```

Runs from our test machine are stored in Artifact_Data. Please see also the corresponding MAPPING.txt file that provides mappings for the figures.

## License

See LICENSE and individual file statements for more information.
