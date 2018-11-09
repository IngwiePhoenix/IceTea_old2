/*
    os-terminal: Testing the terminal API within the $ module.
*/

$.saveDefaultColor()

echo "Now it's time to see some..."
$.setColor($.Colors.RED)
echo " WEIRD "
$.resetColor()
print "Things."

print("")
for(var i,color in $.Colors) {
    echo "Color: "
    $.setColor(color)
    print "${i}"
    $.resetColor()
}
