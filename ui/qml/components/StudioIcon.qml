import QtQuick

Canvas {
    id: root
    property string name: "more"
    property color color: "#B8C7CB"
    implicitWidth: 18
    implicitHeight: 18
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = color
        ctx.fillStyle = color
        ctx.lineWidth = 1.7
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const w = width
        const h = height
        if (name === "add") {
            ctx.beginPath(); ctx.moveTo(w * .5, h * .22); ctx.lineTo(w * .5, h * .78); ctx.moveTo(w * .22, h * .5); ctx.lineTo(w * .78, h * .5); ctx.stroke()
        } else if (name === "more") {
            for (let x of [w * .27, w * .5, w * .73]) { ctx.beginPath(); ctx.arc(x, h * .5, 1.35, 0, Math.PI * 2); ctx.fill() }
        } else if (name === "eye" || name === "eyeOff") {
            ctx.beginPath(); ctx.ellipse(w * .14, h * .27, w * .72, h * .46); ctx.stroke(); ctx.beginPath(); ctx.arc(w * .5, h * .5, 2.1, 0, Math.PI * 2); ctx.fill()
            if (name === "eyeOff") { ctx.beginPath(); ctx.moveTo(w * .2, h * .2); ctx.lineTo(w * .8, h * .8); ctx.stroke() }
        } else if (name === "lock" || name === "unlock") {
            ctx.strokeRect(w * .22, h * .45, w * .56, h * .35)
            ctx.beginPath();
            if (name === "lock") ctx.arc(w * .5, h * .45, w * .19, Math.PI, 0)
            else { ctx.moveTo(w * .31, h * .45); ctx.lineTo(w * .31, h * .28); ctx.arc(w * .5, h * .28, w * .19, Math.PI, 0); ctx.lineTo(w * .69, h * .32) }
            ctx.stroke()
        } else if (name === "mute") {
            ctx.beginPath(); ctx.moveTo(w * .2, h * .43); ctx.lineTo(w * .37, h * .43); ctx.lineTo(w * .58, h * .25); ctx.lineTo(w * .58, h * .75); ctx.lineTo(w * .37, h * .57); ctx.lineTo(w * .2, h * .57); ctx.closePath(); ctx.stroke(); ctx.beginPath(); ctx.moveTo(w * .69, h * .36); ctx.lineTo(w * .86, h * .64); ctx.moveTo(w * .86, h * .36); ctx.lineTo(w * .69, h * .64); ctx.stroke()
        } else if (name === "volume") {
            ctx.beginPath(); ctx.moveTo(w * .18, h * .43); ctx.lineTo(w * .35, h * .43); ctx.lineTo(w * .55, h * .25); ctx.lineTo(w * .55, h * .75); ctx.lineTo(w * .35, h * .57); ctx.lineTo(w * .18, h * .57); ctx.closePath(); ctx.stroke(); ctx.beginPath(); ctx.arc(w * .56, h * .5, w * .2, -Math.PI / 3, Math.PI / 3); ctx.stroke()
        } else if (name === "chevron") {
            ctx.beginPath(); ctx.moveTo(w * .3, h * .4); ctx.lineTo(w * .5, h * .6); ctx.lineTo(w * .7, h * .4); ctx.stroke()
        }
    }
}
