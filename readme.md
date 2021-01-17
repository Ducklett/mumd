Mumd is a one-pass renderer-agnostic markdown parser.

It is a single header library and has a tiny memory footprint (context object + small stacks for nested lists and inline styles).

An example HTML frontend is available in `example.c`

Since Mumd doesn't make any assumptions about your frontend you'll have to handle escaping and syntax highlighting on your own if desired.
