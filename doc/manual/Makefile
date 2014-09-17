# DCP-o-matic manual makefile

all:	html pdf

DIAGRAMS := file-structure.svg 3d-left-right.svg 3d-top-bottom.svg timecode.svg pipeline1.svg pipeline2.svg \
            pipeline3.svg pipeline4.svg

SCREENSHOTS := file-new.png video-new-film.png still-new-film.png video-select-content-file.png \
               still-select-content-file.png examine-thumbs.png examine-content.png timing-tab.png \
               calculate-audio-gain.png add-file.png dcp-tab.png colour-conversion.png \
               prefs-kdm-email.png prefs-colour-conversions.png prefs-metadata.png prefs-general.png prefs-tms.png \
               prefs-advanced.png prefs-defaults.png prefs-servers.png prefs-keys.png \
               making-dcp.png filters.png video-tab.png audio-tab.png subtitles-tab.png timing-tab.png \
               audio-plot.png audio-map-eg1.png audio-map-eg2.png audio-map-eg3.png kdm.png

XML := dcpomatic.xml

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
html/screenshots/making-dcp.png: screenshots/making-dcp.png
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

html:	$(XML) dcpomatic-html.xsl extensions-html.ent dcpomatic.css \
	$(addprefix html/screenshots/,$(SCREENSHOTS)) \
	$(subst .svg,.png,$(addprefix diagrams/,$(DIAGRAMS))) \
	$(subst .svg,.png,$(addprefix graphics/,$(GRAPHICS))) \

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-html.ent extensions.ent

#	DocBoox -> html
	xmlto html -m dcpomatic-html.xsl dcpomatic.xml --skip-validation -o html

#	Copy graphics and CSS in
	mkdir -p html/diagrams
#       mkdir -p html/graphics
	cp diagrams/*.png html/diagrams
#	cp graphics/*.png html/graphics
	cp dcpomatic.css html

#
# PDF
#

pdf:	$(XML) dcpomatic-pdf.xsl extensions-pdf.ent screenshots/*.png $(subst .svg,.pdf,$(addprefix diagrams/,$(DIAGRAMS)))

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p pdf

	dblatex -p dcpomatic-pdf.xsl -s dcpomatic.sty -r pptex.py -T native dcpomatic.xml -t pdf -o pdf/dcpomatic.pdf


#
# LaTeX (handy for debugging)
#

tex:	$(XML) dcpomatic-pdf.xsl extensions-pdf.ent

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p tex

# 	-P <foo> removes the revhistory table
	dblatex -P doc.collab.show=0 -P latex.output.revhistory=0 -p dcpomatic-pdf.xsl -s dcpomatic.sty -r pptex.py -T native dcpomatic.xml -t tex -o tex/dcpomatic.tex


clean:;	rm -rf html pdf diagrams/*.pdf diagrams/*.png graphics/*.png *.aux dcpomatic.cb dcpomatic.cb2 dcpomatic.glo dcpomatic.idx dcpomatic.ilg
	rm -rf dcpomatic.ind dcpomatic.lof dcpomatic.log dcpomatic.tex dcpomatic.toc extensions.ent dcpomatic.out
