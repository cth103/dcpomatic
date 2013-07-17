<?xml version='1.0' encoding="iso-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>

<!-- colour links in black -->
<xsl:param name="latex.hyperparam">colorlinks,linkcolor=black,urlcolor=black</xsl:param>

<!-- no revhistory table -->
<xsl:param name="doc.collab.show">0</xsl:param>
<xsl:param name="latex.output.revhistory">0</xsl:param>

<!-- hack images to vaguely the right size -->
<xsl:param name="imagedata.default.scale">scale=0.6</xsl:param>

<!-- don't make too-ridiculous section numbers -->
<xsl:param name="doc.section.depth">3</xsl:param>

</xsl:stylesheet>
