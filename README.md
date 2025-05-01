# SCOT

## Building

```
cd SCOT
make
```

## Usage Example

Test linked list (Harris-Michael, Harris' linked lists). In the example below, 10 is 10 seconds, 16 is the key range and number of elements, 16 is the key range, 5 is the number of iterations, 50 is the percentage of reads, 25 is the percentage of inserts, 25 is the percentage of deletes, IBR is thereclamation scheme (can also be NR, EBR, HP).

```
./SCOT/bench list 10 16 5 50 25 25 IBR
```

## Running tests

If you go to Scripts, you can run a full-blown test:

```
nohup ./source.sh &
```

or you can run a lightweight (1 iteration) test:

```
nohup ./source_lightweight.sh &
```

## Data

When running the experiment scripts, they will put output files into the Data directory. You can then generate charts by running:

```
python3 generate_charts.py

```

## License

See LICENSE and individual file statements for more information.
