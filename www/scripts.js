
var currentPage = "empty";
var currentInfo = "";

//  Update callbacks
var UpdateData = {};
//  Update counter. Sent to server which determines update
var UpdateId = 0;

var NumScoreTables = 3;
var CurrentSound = undefined;

var PlaySoundsQueue = [];
var LoadedSounds = {};

var TimeoutInSeconds = undefined;
var TimeoutIntervalId = undefined

TimeoutHandler = function () {
    if (TimeoutInSeconds !== undefined) {
        TimeoutInSeconds -= 1
        UpdateTimeout();
    }
}

UpdateTimeout = function () {
    timeout = "";
    if (TimeoutInSeconds !== undefined) {
        if (TimeoutInSeconds <= 600) {
            timeout += TimeoutInSeconds;
        }
    }
    $("#Timeout").html(timeout);
}

SetInfoText = function (text) {
    if (text != currentInfo) {
        currentInfo = text;
        $("#InformationText").html(currentInfo);
    }
    if (text == "") {
        $("#Information").hide()
    }
    else {
        $("#Information").show()
    }
    UpdateTimeout();
}

WalkRequiredSounds = function (onFileNotLoadedCallback) {
    var sounds = PlaySoundsQueue.slice();
    while (sounds.length != 0) {
        var Sound = sounds.shift();
        if (typeof Sound != "string") {
            Sound = sounds.shift();
        }
        if (LoadedSounds[Sound] === undefined) {
            onFileNotLoadedCallback(Sound);
        }
    }
}

PlayNextSound = function () {
    console.log("PlayNextSound");
    if (PlaySoundsQueue.length) {
        var Sound = PlaySoundsQueue.shift();
        var Delay = 0;
        if (typeof Sound == "string") {
        } else {
            Delay = Sound;
            Sound = PlaySoundsQueue.shift();
        }
        console.log("Play sound" + Sound)
        CurrentSound = createjs.Sound.play(Sound);
        CurrentSound.addEventListener('failed', function () {
            console.log("Failed to play sound");
        });
        CurrentSound.addEventListener('complete', function () {
            PlayNextSound();
        });
        CurrentSound.play({ delay: Delay });
    }
    else {
        CurrentSound = undefined;
    }
}

PlaySounds = function (sounds) {
    //  Stop any current sounds
    PlaySoundsQueue = [];
    if (CurrentSound !== undefined) {
        CurrentSound.stop();
        CurrentSound = undefined;
    }
    sounds = sounds || [];
    PlaySoundsQueue = sounds;

    var Complete = true;
    WalkRequiredSounds(function (Sound) {
        createjs.Sound.registerSound(Sound);
        Complete = false;
    });
    if (Complete) {   
    //  start playback immediately if all sounds are preloaded
        PlayNextSound();
    }
}

UpdateGameData = function (data) {
    if (data.Reload) {
        location.reload(true);
        return;
    }

    TimeoutInSeconds = data.TimeoutInSeconds;
    //  restart interval. current number should be more precise
    UpdateTimeout();
    clearInterval(TimeoutIntervalId);
    TimeoutIntervalId = setInterval(TimeoutHandler, 1000);

    if (UpdateId != data.UpdateId) {
        //  update only current screen, looks shitty if during animation data in previous screen is updated
        var screenChanged = DisplayPage(data.screen)

        //  per screen special update
        if (data.screen in UpdateData) {
            UpdateData[data.screen](data, screenChanged);
        }

        var screen = "#" + data.screen + " ";
        UpdateId = data.UpdateId;
        $(screen + ".gameMode").html(data.gameMode);
        $(screen + ".soundSetName").html(data.soundSetName);
        $(screen + ".player0name").html(data.players[0].name);
        $(screen + ".player1name").html(data.players[1].name);
        $(screen + ".player0score").html(data.players[0].points);
        $(screen + ".player1score").html(data.players[1].points);
        $(screen + ".player0sets").html(data.players[0].sets);
        $(screen + ".player1sets").html(data.players[1].sets);
        $(screen + ".player0serves").html(data.players[0].serves ? '●' : '');
        $(screen + ".player1serves").html(data.players[1].serves ? '●' : '');
        SetInfoText(data.info);
        //  play sounds if specified by server
        if (data.Sounds !== undefined) {
            PlaySounds(data.Sounds);
        } else {
            PlaySounds([]);
        }
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

UpdateData.configuration = function (data, pageChanged) {
    if (data.configuration) {
        var html = "<table>";
        var Configuration = data.configuration;
        var index;
        for (index = 0; index < Configuration.length; ++index) {
            var option = Configuration[index];
            html += '<tr' + (option.active ? ' class="selected"' : "") + '><td>' + option.name + '</td><td><span class="' + option.replace + '"></span></td></tr>';
        }
        html += "</table>";
        $("#configuration").html(html);
    }
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
                    html += '<td class="Date nowrap" rowspan="2"><div>' + (new Date(match.datetime * 1000)).toLocaleDateString() + "</div></td>";
                }
                html += '<td class="Win">' + (match.win[row] ? '<div class="glyphicon glyphicon-star"></div>' : '<div></div>') + "</td>";
                html += '<td class="Name nowrap"><div>' + match.names[row] + "</div></td>";
                var set;
                for (set = 0; set < match.sets.length; ++set) {
                    html += '<td class="Score"><div>' + match.sets[set][row] + "</div></td>";
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

$(document).ready(function () {
    $(".screen").hide();
    DisplayPage("wait");
    RequestAndUpdatePageData();
    TimeoutIntervalId = setInterval(TimeoutHandler, 1000);
    createjs.Sound.addEventListener('fileload', function (event) {
        var CompleteBefore = true;
        WalkRequiredSounds(function (Sound) {
            CompleteBefore = false;
        });
        LoadedSounds[event.src] = true;
        var CompleteAfter = true;
        WalkRequiredSounds(function (Sound) {
            CompleteAfter = false;
        });
        //  If we have all sounds after and not all before play
        if (CompleteAfter && !CompleteBefore) {
            PlayNextSound();
        }
    });
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
