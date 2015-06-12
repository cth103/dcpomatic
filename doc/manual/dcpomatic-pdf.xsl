<?xml version='1.0' encoding="iso-8859-1"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fo="http://www.w3.org/1999/XSL/Format" version='1.0'>

<!-- colour links in blue -->
<xsl:param name="latex.hyperparam">colorlinks,linkcolor=blue,urlcolor=blue</xsl:param>

<!-- no revhistory table -->
<xsl:param name="doc.collab.show">0</xsl:param>
<xsl:param name="latex.output.revhistory">0</xsl:param>

<!-- hack images to vaguely the right size -->
<xsl:param name="imagedata.default.scale">scale=0.5</xsl:param>

<!-- don't make too-ridiculous section numbers -->
<xsl:param name="doc.section.depth">3</xsl:param>

<!-- guilabel in sans-serif -->
<xsl:template name="inline.frobozz">
  <xsl:param name="content">
    <xsl:apply-templates/>
  </xsl:param>
  <xsl:text>\textsf{</xsl:text>
  <xsl:copy-of select="$content"/>
  <xsl:text>}</xsl:text>
</xsl:template>

<xsl:template match="guibutton|guiicon|guilabel|guimenu">
  <xsl:call-template name="inline.frobozz"/>
</xsl:template>

</xsl:stylesheet>
