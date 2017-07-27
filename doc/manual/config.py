import sys
current_tag = None
current_doc = None
current_opt = False
print '<itemizedlist>'
with open(sys.argv[1]) as f:
    for line in f:
        line = line.strip();
        if line.startswith('/* [XML'):
            code = line.split()[1]
            current_tag = line.split()[2]
            current_opt = code == '[XML:opt]'
            line = line[5+len(code)+len(current_tag):]
            current_doc = ''
        if current_tag is not None:
            current_doc += line.replace('/* [XML] ', '').replace('*/', '').strip() + ' '
        if line.find('*/') != -1 and current_tag is not None:
            if current_opt:
                optional = ' (optional)'
            else:
                optional = ''
            print '<listitem><code>&lt;%s&gt;</code>%s &#8212; %s</listitem>' % (current_tag, optional, current_doc.strip())
            current_tag = None
print '</itemizedlist>'
