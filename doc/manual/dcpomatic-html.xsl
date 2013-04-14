<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                version="1.0">

<!-- Our CSS -->
<xsl:param name="html.stylesheet" select="'dcpomatic.css'"/>

<!-- I can't fathom xmlto's logic with image scaling, so I've turned it off -->
<xsl:param name="ignore.image.scaling" select="1"/>

</xsl:stylesheet>
