## Axes ordering

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
