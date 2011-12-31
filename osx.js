var osx = require('./build/Release/osx');

module.exports.mountInfo = osx.mountInfo;
module.exports.accounts = osx.accounts;
module.exports.who = osx.who;
module.exports.procs = osx.procs;
module.exports.interfaces = osx.interfaces;
module.exports.networkActivity = osx.networkActivity;

module.exports.procArgs = function(pid) {
    if (typeof pid == 'number') {
        return osx.procArgs(pid);
    } else {
        throw new Error('pid must be a number');
    }
};

module.exports.users = function() {
    var usrs = osx.who();
    var uniq = {};
    var ret = [];
    for (var i in usrs) {
        uniq[usrs[i].name] = usrs[i].name;
    }
    for (var name in uniq) {
        ret.push(name);
    }
    return ret;
};

