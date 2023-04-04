# RcoDecompiler
The tools to properly decompile PS Vita .rco files

# Special Thanks

[Graphene](https://github.com/GrapheneCt) for Useful Cxml Information.

# How to use

<code>./RcoDecompiler ./your_plugin.rco</code> is output to <code>./your_plugin/your_plugin.xml</code> on same directory.

# Known issues

- Lacks a lot of error checking.
- If the files contained in the .rco contain compressed data, they will all be uncompressed. So it will be inconsistent with the .xml compress key.
- Some code is not in the right place. For example the code to print the src file should be outside of print_xml (search "process_stringtable" on src code).
