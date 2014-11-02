
var LogonsSelector = "#Logons";
var UserIdsSelector = "#UserId"
var UserNameSelector = "#UserName"


function ReloadLogons() {
    SendCommand({ "LastLogons": 10 }, function (data) {
        var LastLogons = data.LastLogons;
        var html = "";
        if (LastLogons !== undefined) {
            var idx = 0;
            html += "<tr>";
            html += "<td>Name</td><td>ID</td><td>Last login</td>"

            for (idx = 0; idx < LastLogons.length; ++idx) {
                var Logon = LastLogons[idx];
                html += '<tr onclick="NavigateToUserManagement(\'' + Logon.ID + '\');">';
                html += "<td>" + Logon.Name + "</td>";
                html += "<td>" + Logon.ID + "</td>";
                html += "<td>" + new Date(Logon.LastLogon * 1000).toLocaleString() + "</td>";
                html += "</tr>";
            }
            $(LogonsSelector).html(html);
        }
    });
}

function NavigateToUserManagement(userId) {
    NavigateToAccordion('#UserManagement');
    if (userId !== undefined) {
        $(UserIdsSelector).val(userId);
    }
    $(UserIdsSelector).trigger('change');
}

function NavigateToAccordion(divSelector) {
    $(divSelector).prev("h3").click();
}

function ReloadUsers(selectedId) {
    SendCommand({ "Users": "x" }, function (data) {
        var Users = data.Users;
        var html = "";
        if (Users !== undefined) {
            var idx = 0;

            for (idx = 0; idx < Users.length; ++idx) {
                var User = Users[idx];
                html += '<option name="' + User.Name + '" value="' + User.ID + '"' + (idx ? '' : ' selected') + '>' + User.ID + '</option>'
            }
        }
        $(UserIdsSelector).html(html);
        if (selectedId !== undefined) {
            $(UserIdsSelector).val(selectedId);
        }
        $(UserIdsSelector).trigger('change');
    });
}

function RenameUser(Id, Name) {
    SendCommand({ "RenameUser": { "ID": Id, "Name": Name} }, function (data) {
        $(UserNameSelector).val(Name);
    });
    ReloadUsers(Id);
}

function SendCommand(command, complete) {
    SendRPC({"command": command }, complete);
}

function SendRPC(message, complete) {
    var rpc = {
        "url": "/rpc/command",
        "cache":false
    };
    rpc.type = "PUT";
    rpc.data = JSON.stringify(message);
    rpc.dataType = 'json';
    $.ajax(rpc)
        .done(function (data) {
            if (data.error) {
                alert(data.error);
            }
            else
            if (complete) {
                complete(data)
            }
        })
        .error(function () {
            alert("Error");
        });
}
