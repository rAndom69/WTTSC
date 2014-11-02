
var currentPage = "empty";
var currentInfo = "";

//  Update callbacks
var UpdateData = {};
//  Update counter. Sent to server which determines update
var UpdateId = 0;

var NumScoreTables = 3;

SetInfoText = function (text) {
    if (text != currentInfo) {
        currentInfo = text;
        $("#info").html(currentInfo);
    }
}

UpdateGameData = function (data) {
    if (data.Reload) {
        location.reload(true);
        return;
    }

    if (UpdateId != data.UpdateId) {
        //  update only current screen, looks shitty if during animation data in previous screen is updated
        var screenChanged = DisplayPage(data.screen)
        var screen = "#" + data.screen + " ";
        UpdateId = data.UpdateId;
        $(screen + ".gameMode").html(data.gameMode);
        $(screen + ".player0name").html(data.players[0].name);
        $(screen + ".player1name").html(data.players[1].name);
        $(screen + ".player0score").html(data.players[0].points);
        $(screen + ".player1score").html(data.players[1].points);
        $(screen + ".player0sets").html(data.players[0].sets);
        $(screen + ".player1sets").html(data.players[1].sets);
        $(screen + ".player0serves").html(data.players[0].serves ? '<div class="scoreBoardMiddlePasses symbol">●</div>' : '');
        $(screen + ".player1serves").html(data.players[1].serves ? '<div class="scoreBoardMiddlePasses symbol">●</div>' : '');
        SetInfoText(data.info);
        //  per screen special update
        UpdateData[data.screen](data, screenChanged);
    }
}

RandomEffectForScore = function () {
    switch (Math.floor(Math.random() * 4)) {
        case 0: return [{ name: "fade", parameters: {}},
                        { name: "fade", parameters: {}}];
        case 1: return [{ name: "drop", parameters: { direction: "left"} },
                        { name: "drop", parameters: { direction: "right" }}];
        case 2: return [{name: "bounce", parameters: {}},
                        {name: "bounce", parameters: {}}];
        case 3: return [{name: "clip", parameters: { direction: "horizontal"} },
                        {name: "clip", parameters: { direction: "horizontal"}}];
    };
}

RandomEffectForPage = function () {
    switch (Math.floor(Math.random() * 3)) {
        case 0: return [{ name: "drop", parameters: { direction: "left"} },
                        { name: "fade", parameters: {}}];
        case 1: return [{ name: "drop", parameters: { direction: "right"} },
                        { name: "fade", parameters: {}}];
        case 2: return [{ name: "puff", parameters: {} },
                        { name: "fade", parameters: {}}];
    };
}

UpdateData.idle = function (data, pageChanged) {
    if (data.scoreTable) {
        //  display score table. displayed state should be kept until new state arrives
        var ScoreTable = data.scoreTable;
        var index;
        var htmlColumns = [];
        var html = "";
        for (index = 0; index < NumScoreTables; ++index) {
            htmlColumns[index] = '<div class="scoreColumn" style="width:' + (100 / NumScoreTables) + '%;"><table>';
        }
        for (index = 0; index < ScoreTable.length; ++index) {
            html = "";
            var match = ScoreTable[index];
            var row;
            for (row = 0; row < 2; ++row) {
                html += "<tr>";
                if (row == 0) {
                    html += '<td class="nowrap" rowspan="2">' + (new Date(match.datetime * 1000)).toLocaleDateString() + "</td>";
                }
                html += '<td class="Win symbol">' + (match.win[row] ? '★' : '') + "</td>";
                html += '<td class="Name nowrap">' + match.names[row] + "</td>";
                var set;
                for (set = 0; set < match.sets.length; ++set) {
                    html += '<td class="Score">' + match.sets[set][row] + "</td>";
                }
                html += "</tr>";
            }
            html += '<tr><td class="Spacer"><td></tr>'
            htmlColumns[index % NumScoreTables] += html;
        }
        html = '<div class="scoreTitle GradientRed">' + data.scoreText + '</div>';
        for (index = 0; index < NumScoreTables; ++index) {
            htmlColumns[index] += '</table></div>';
            html += htmlColumns[index];
        }
        if (pageChanged) {
            $("#highscore").html(html);
        }
        else {
            $("#highscoreBackup").html($(".scoreTable").html()).show();
            $("#highscore").html(html).hide();
            var effect = RandomEffectForScore();
            effect[0].parameters.complete = function () {
                $("#highscore").show(effect[1].name, effect[1].parameters);
            };
            $("#highscoreBackup").hide(effect[0].name, effect[0].parameters);
        }
    }
}

UpdateData.game = function (data) {
}

UpdateData.wait = function (data) {
}

UpdateData.empty = function (data) {
}

$(document).ready(function () {
    $(".screen").hide();
    DisplayPage("wait");
    RequestAndUpdatePageData();
});

function DisplayPage(newPage) {
    var pageChanged = false;
    if (newPage != currentPage) {
        var effect = RandomEffectForPage();
        $("#" + currentPage).hide(effect[0].name, effect[0].parameters);
        $("#" + newPage).show(effect[1].name, effect[1].parameters);
        pageChanged = true;
    }
    currentPage = newPage;
    return pageChanged;
}

function RequestAndUpdatePageData(message) {
    var rpc = {
        "url": "/rpc/state",
        "cache":false
    };
    //  Always send last update of score table
    if (message === undefined) {
        message = {};
    }
    message.UpdateId = UpdateId;
    rpc.type = "PUT";
    rpc.data = JSON.stringify(message);
    rpc.dataType = 'json';

    $.ajax(rpc)
        .done(function (data) {
            UpdateGameData(data);
            setTimeout(RequestAndUpdatePageData, 0);
        })
        .error(function () {
            //  retry indefinitely
            DisplayPage("wait");
            SetInfoText("No server connection.");
            RequestAndUpdatePageData(message);
        });
}
