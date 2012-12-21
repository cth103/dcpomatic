# DVD-o-matic manual makefile

all:	html pdf

DIAGRAMS := 

SCREENSHOTS := file-new.png video-new-film.png still-new-film.png click-content-selector.png video-select-content-file.png \
               still-select-content-file.png examine-thumbs.png range.png \
               calculate-audio-gain.png prefs.png making-dcp.png filters.png film-tab.png video-tab.png audio-tab.png subtitles-tab.png

XML := dvdomatic.xml

GRAPHICS := 

#
# For the HTML, default to copying the screenshots direct
#
html/screenshots/%.png: screenshots/%.png
	mkdir -p html/screenshots
	cp $< $@

#
# Some need resizing...
#
html/screenshots/editor.png: screenshots/editor.png
	mkdir -p html/screenshots
	convert -resize 50% $< $@
html/screenshots/mixer.png: screenshots/mixer.png
	mkdir -p html/screenshots
	convert -resize 50% $< $@
html/screenshots/default-folder-for-new-sessions.png: screenshots/default-folder-for-new-sessions.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/welcome-to-ardour.png: screenshots/welcome-to-ardour.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/monitoring-choices.png: screenshots/monitoring-choices.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/monitor-section.png: screenshots/monitor-section.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/audio-midi-setup-device.png: screenshots/audio-midi-setup-device.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/new-session.png: screenshots/new-session.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@
html/screenshots/export-dialogue.png: screenshots/export-dialogue.png
	mkdir -p html/screenshots
	convert -resize 75% $< $@

# For HTML: convert graphics from SVG to PNG
graphics/%.png:	graphics/%.svg
	inkscape -z -f $< --export-png $@ --export-area-drawing

# For LaTeX/PDF: convert graphics from SVG to PDF
graphics/%.pdf:	graphics/%.svg
	inkscape -z -f $< --export-pdf $@ --export-area-drawing

# For HTML: convert diagrams from SVG to PNG
diagrams/%.png:	diagrams/%.svg
	inkscape -z -f $< --export-png $@ --export-area-drawing

# For LaTeX/PDF: convert diagrams from SVG to PDF
diagrams/%.pdf:	diagrams/%.svg
	inkscape -z -f $< --export-pdf $@ --export-area-drawing

#
# HTML
#

html:	$(XML) dvdomatic-html.xsl extensions-html.ent dvdomatic.css \
	$(addprefix html/screenshots/,$(SCREENSHOTS)) \
	$(subst .svg,.png,$(addprefix diagrams/,$(DIAGRAMS))) \
	$(subst .svg,.png,$(addprefix graphics/,$(GRAPHICS))) \

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-html.ent extensions.ent

#	DocBoox -> html
	xmlto html -m dvdomatic-html.xsl dvdomatic.xml --skip-validation -o html

#	Copy graphics and CSS in
#	mkdir -p html/diagrams html/graphics
#	cp diagrams/*.png html/diagrams
#	cp graphics/*.png html/graphics
	cp dvdomatic.css html

#
# PDF
#

pdf:	$(XML) dvdomatic-pdf.xsl extensions-pdf.ent screenshots/*.png $(subst .svg,.pdf,$(addprefix diagrams/,$(DIAGRAMS)))

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p pdf

	dblatex -p dvdomatic-pdf.xsl -s dvdomatic.sty -r pptex.py -T native dvdomatic.xml -t pdf -o pdf/dvdomatic.pdf


#
# LaTeX (handy for debugging)
#

tex:	$(XML) dvdomatic-pdf.xsl extensions-pdf.ent

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p tex

# 	-P <foo> removes the revhistory table
	dblatex -P doc.collab.show=0 -P latex.output.revhistory=0 -p dvdomatic-pdf.xsl -s dvdomatic.sty -r pptex.py -T native dvdomatic.xml -t tex -o tex/dvdomatic.tex


clean:;	rm -rf html pdf diagrams/*.pdf diagrams/*.png graphics/*.png *.aux dvdomatic.cb dvdomatic.cb2 dvdomatic.glo dvdomatic.idx dvdomatic.ilg
	rm -rf dvdomatic.ind dvdomatic.lof dvdomatic.log dvdomatic.tex dvdomatic.toc extensions.ent dvdomatic.out
