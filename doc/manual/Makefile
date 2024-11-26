# DCP-o-matic manual makefile

INKSCAPE = ~/Applications/inkscape
LIBDCP = ~/src/libdcp


all:	html pdf

DIAGRAMS := file-structure.svg 3d-left-right.svg 3d-top-bottom.svg timecode.svg pipeline1.svg pipeline2.svg \
            pipeline3.svg pipeline4.svg burn-in.svg discrete.svg dcp-copy.svg dcp-refer.svg reels-by-video.svg \
	    crypt.svg

SCREENSHOTS := file-new.pdf new-film.pdf video-select-content-file.pdf \
               still-select-content-file.pdf examine-content.pdf timing-tab.pdf \
               calculate-audio-gain.pdf add-file.pdf dcp-tab.pdf colour-conversion.pdf \
               prefs-email.pdf prefs-kdm-email.pdf prefs-general.pdf prefs-tms.pdf prefs-cover-sheet.pdf \
               prefs-advanced.pdf prefs-defaults.pdf prefs-servers.pdf prefs-keys.pdf \
               prefs-notifications.pdf timeline1.pdf timed-text-tab.pdf \
               making-dcp.pdf filters.pdf video-tab.pdf audio-tab.pdf \
               audio-plot.pdf audio-map-eg1.pdf audio-map-eg2.pdf audio-map-eg3.pdf kdm.pdf \
               kdm-creator.pdf export.pdf advanced-content.pdf disk-writer-notice.pdf disk-writer.pdf \
               markers.pdf prefs-notifications.pdf prefs-cover-sheet.pdf add-screen.pdf \
	       advanced-player.pdf playlist-editor.pdf playlist-editor-prefs.pdf

XML := dcpomatic.xml

#
# For the HTML: convert PDF screenshots to PNG and copy into html/screenshots, scaling some.
#
html/screenshots/%.png: screenshots/%.pdf
	mkdir -p html/screenshots
	pdftoppm $< $(basename $@) -png -f 1 -singlefile -rx 80 -ry 80


# For LaTeX/PDF: crop some of the PDF screenshots; use others as-is
screenshots/%.pdf: raw-screenshots/%.pdf
	cp $< $@

screenshots/prefs-general.pdf: raw-screenshots/prefs-general.pdf
	python3 pdf_crop_by.py $< $@ 0 400 0 0

screenshots/prefs-defaults.pdf: raw-screenshots/prefs-defaults.pdf
	python3 pdf_crop_by.py $< $@ 0 400 0 0

screenshots/prefs-servers.pdf: raw-screenshots/prefs-servers.pdf
	python3 pdf_crop_by.py $< $@ 0 700 0 0

screenshots/prefs-keys.pdf: raw-screenshots/prefs-keys.pdf
	python3 pdf_crop_by.py $< $@ 0 600 0 0

screenshots/prefs-tms.pdf: raw-screenshots/prefs-tms.pdf
	python3 pdf_crop_by.py $< $@ 0 600 0 0

screenshots/prefs-email.pdf: raw-screenshots/prefs-email.pdf
	python3 pdf_crop_by.py $< $@ 0 700 0 0

screenshots/prefs-cover-sheet.pdf: raw-screenshots/prefs-cover-sheet.pdf
	python3 pdf_crop_by.py $< $@ 0 600 0 0

screenshots/examine-content.pdf: raw-screenshots/examine-content.pdf
	python3 pdf_crop_by.py $< $@ 460 270 200 580

screenshots/audio-tab.pdf: raw-screenshots/audio-tab.pdf
	python3 pdf_crop_by.py $< $@ 0 0 1480 420

screenshots/timing-tab.pdf: raw-screenshots/timing-tab.pdf
	python3 pdf_crop_by.py $< $@ 0 100 1475 420

screenshots/timed-text-tab.pdf: raw-screenshots/timed-text-tab.pdf
	python3 pdf_crop_by.py $< $@ 0 50 1480 420

screenshots/video-tab.pdf: raw-screenshots/video-tab.pdf
	python3 pdf_crop_by.py $< $@ 0 0 1480 365

screenshots/add-file.pdf: raw-screenshots/add-file.pdf
	python3 pdf_crop_by.py $< $@ 0 600 1210 0

screenshots/making-dcp.pdf: raw-screenshots/making-dcp.pdf
	python3 pdf_crop_by.py $< $@ 470 190 300 700

screenshots/audio-map-eg1.pdf: raw-screenshots/audio-map-eg1.pdf
	python3 pdf_crop_by.py $< $@ 10 195 1350 630

screenshots/audio-map-eg2.pdf: raw-screenshots/audio-map-eg2.pdf
	python3 pdf_crop_by.py $< $@ 10 195 1350 630

screenshots/audio-map-eg3.pdf: raw-screenshots/audio-map-eg3.pdf
	python3 pdf_crop_by.py $< $@ 10 75 1350 630

screenshots/dcp-tab.pdf: raw-screenshots/dcp-tab.pdf
	python3 pdf_crop_by.py $< $@ 0 350 1240 30


# For HTML: convert diagrams from SVG to PNG
diagrams/%.png:	diagrams/%.svg
	$(INKSCAPE) $< --export-type=png --export-filename=$@ --export-area-drawing

# For LaTeX/PDF: convert diagrams from SVG to PDF
diagrams/%.pdf:	diagrams/%.svg
	$(INKSCAPE) $< --export-type=pdf --export-filename=$@ --export-area-drawing

config.xml:	../../src/lib/config.cc config.py
	python3 config.py ../../src/lib/config.cc > config.xml

SHORTCUTS :=	../../src/tools/dcpomatic.cc

shortcuts.xml:	$(SHORTCUTS) shortcuts.py
	python3 shortcuts.py $(SHORTCUTS) > $@

LIBDCP_DEPS = $(LIBDCP)/src/verify_j2k.cc $(LIBDCP)/src/dcp.cc $(LIBDCP)/src/verify.cc

verify_errors.xml:	verifier.py $(LIBDCP_DEPS)
	python3 verifier.py $(LIBDCP) ERROR > $@

verify_bv21_errors.xml:	verifier.py $(LIBDCP_DEPS)
	python3 verifier.py $(LIBDCP) BV21_ERROR > $@

verify_warnings.xml:	verifier.py $(LIBDCP_DEPS)
	python3 verifier.py $(LIBDCP) WARNING > $@

verify_ok.xml:	verifier.py $(LIBDCP_DEPS)
	python3 verifier.py $(LIBDCP) OK > $@

#
# HTML
#

html:	$(XML) config.xml shortcuts.xml verify_errors.xml verify_bv21_errors.xml verify_warnings.xml verify_ok.xml \
	dcpomatic-html.xsl extensions-html.ent dcpomatic.css dcpomatic_create.xml dcpomatic_cli.xml dcpomatic_kdm_cli.xml \
	$(subst .pdf,.png,$(addprefix html/screenshots/,$(SCREENSHOTS))) \
	$(subst .svg,.png,$(addprefix diagrams/,$(DIAGRAMS))) \

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-html.ent extensions.ent

#	DocBoox -> html
	xmlto html -m dcpomatic-html.xsl dcpomatic.xml --skip-validation -o html

#	Copy graphics and CSS in
	mkdir -p html/diagrams
	cp diagrams/*.png html/diagrams
	cp dcpomatic.css html

#
# PDF
#

pdf:	$(XML) config.xml shortcuts.xml verify_errors.xml verify_bv21_errors.xml verify_warnings.xml verify_ok.xml \
	dcpomatic-pdf.xsl extensions-pdf.ent dcpomatic_create.xml dcpomatic_cli.xml dcpomatic_kdm_cli.xml \
	$(addprefix screenshots/,$(SCREENSHOTS)) \
	$(subst .svg,.pdf,$(addprefix diagrams/,$(DIAGRAMS)))

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p pdf

	dblatex -p dcpomatic-pdf.xsl -s dcpomatic.sty -r pptex.py -T native dcpomatic.xml -t pdf -o pdf/dcpomatic.pdf


#
# LaTeX (handy for debugging)
#

tex:	$(XML) config.xml dcpomatic-pdf.xsl extensions-pdf.ent

# 	The DocBook needs to know what file extensions to look for
# 	for screenshots and diagrams; use the correct file to tell it.
	cp extensions-pdf.ent extensions.ent

	mkdir -p tex

# 	-P <foo> removes the revhistory table
	dblatex -P doc.collab.show=0 -P latex.output.revhistory=0 -p dcpomatic-pdf.xsl -s dcpomatic.sty -r pptex.py -T native dcpomatic.xml -t tex -o tex/dcpomatic.tex


clean:;	rm -rf html pdf diagrams/*.pdf diagrams/*.png graphics/*.png *.aux dcpomatic.cb dcpomatic.cb2 dcpomatic.glo dcpomatic.idx dcpomatic.ilg
	rm -rf dcpomatic.ind dcpomatic.lof dcpomatic.log dcpomatic.tex dcpomatic.toc extensions.ent dcpomatic.out
