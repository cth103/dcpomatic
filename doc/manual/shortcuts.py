import sys

shortcuts = []

for filename in sys.argv[1:]:
    with open(filename) as f:
        for line in f:
            line = line.strip();
            if line.startswith('/* [Shortcut] '):
                desc = line[14:-2].strip()
                parts = desc.split(':')
                shortcuts.append(desc.split(':'))

shortcuts.sort(key=lambda x: x[0])

print("""
<table id="keyboard shortcuts">
  <title>Keyboard shortcuts</title>
  <tgroup cols='2' align='left' colsep='1' rowsep='1'>
    <thead>
      <row>
	<entry>Key</entry>
	<entry>Action</entry>
      </row>
    </thead>
    <tbody>
""")

for s in shortcuts:
    print("      <row>")
    print(f"        <entry>{s[0]}</entry>")
    print(f"        <entry>{s[1]}</entry>")
    print("      </row>")

print("""
    </tbody>
  </tgroup>
</table>
""")

