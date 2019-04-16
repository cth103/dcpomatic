all:			sizing_card_flat sizing_card_scope

sizing_card_flat:	sizing_card_flat.png
			rm -rf sizing_card_flat
			mkdir sizing_card_flat
			/usr/bin/dcpomatic2_create --content-ratio 185 $< > $@/metadata.xml
			/usr/bin/dcpomatic2_cli $@

sizing_card_scope:	sizing_card_scope.png
			rm -rf sizing_card_scope
			mkdir sizing_card_scope
			/usr/bin/dcpomatic2_create --content-ratio 239 $< > $@/metadata.xml
			/usr/bin/dcpomatic2_cli $@

sizing_card_%.png:	sizing_card_%.svg
			inkscape -z -C -e $@ $<

clean:;			rm -rf sizing_card_flat sizing_card_scope sizing_card_flat.png sizing_card_scope.png
