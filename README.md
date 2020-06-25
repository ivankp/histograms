# Axes ordering

The order of axes is little endian in the array of bin.
This means that axes are specified in order from innermost to outermost.
For example, consider a histogram that has two axes,
```
histograms::histogram h({
  {0,1,2,3,4,5},
  {1,10,100}
});

```
The first axis (at index `0`) has 7 bins, and
the second axis (at index `1`) has 4 bins.
The bin corresponding to coordinate (4.5,15) has indices `[5,2]`,
or a joint index `5 + 2*7 == 19`.
The first axis is the innermost.

# JSON serialization specification

## Histograms

```JSON
{
  "axes": [ . . . ],
  "bins": [ . . . ],
  "hists": {
    "first_histogram": {
      "axes": [ . . . ],
      "bins": [ def, [ . . . ] ]
    }
  }
}
```

- Any other fields can be added, as long as they are not called
  `"axes"`, or `"bins"`.

- Elements of `"axes"` can be either in-place axis definitions or numbers
  referring to the index in the global `"axes"` array.

- For each histogram, the first element of the `"bins"` array is either the
  bin structure definition, or a number referring to the index in the global
  `"bins"` array.

  The second element is an array of serialized bin structure representations,
  corresponding to the format defined in the first element.

  Histogram definitions can use abbreviated form `"bins": def`.

## Axes

```JSON
[ 1, 5, 10, 50, 100 ]
```
an axis with the given bin edges.

```JSON
[ [ 0, 1, 10 ] ]
```
an axis with 10 internal bins, i.e. 11 edges, between 0 and 1.

Optional subsequent elements provide extra specifications.
For example, a logarithmically binned axis can be specified with `"log"`.
```JSON
[ [ 1e-10, 1e5, 15, "log" ] ]
```

The two formats can be combined, e.g.
```JSON
[ [ 0, 1, 10 ], 50, [ 100, 1000, 9, "log" ] ]
```
