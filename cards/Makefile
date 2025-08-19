DCPOMATIC_CREATE := 	../run/dcpomatic_create
DCPOMATIC_CLI := 	../run/dcpomatic_cli

all:			sizing_card_flat.zip sizing_card_scope.zip

sizing_card_%.zip:	sizing_card_%
			cd $< && zip -9 -r ../$@ SizingCard*

sizing_card_flat:	sizing_card_flat.png
			rm -rf sizing_card_flat
			mkdir sizing_card_flat
			$(DCPOMATIC_CREATE) --container-ratio 185 $(CURDIR)/$< > $(CURDIR)/$@/metadata.xml
			$(DCPOMATIC_CLI) $(CURDIR)/$@

sizing_card_scope:	sizing_card_scope.png
			rm -rf sizing_card_scope
			mkdir sizing_card_scope
			$(DCPOMATIC_CREATE) --container-ratio 239 $(CURDIR)/$< > $(CURDIR)/$@/metadata.xml
			$(DCPOMATIC_CLI) $(CURDIR)/$@

sizing_card_%.png:	sizing_card_%.svg
			inkscape --export-filename $@ $<

clean:;			rm -rf sizing_card_flat sizing_card_scope sizing_card_flat.png sizing_card_scope.png
