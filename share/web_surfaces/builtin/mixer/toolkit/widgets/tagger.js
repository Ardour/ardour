(function (w, TK) {
"use strict"

function keyup (e) {
    if (e.keyCode != 13) return;
    new_tag_from_input.call(this);
}
function addclicked (e) {
    new_tag_from_input.call(this);
}
function new_tag_from_input () {
    var val = this._input.value;
    if (!val) return;
    this._input.value = "";
    var t = false;
    if (!this.options.async)
        t = this.add_tag(val);
    this.fire_event("newtag", val, t);
    if (this.options.closenew)
        this.close();
}

TK.Tagger = TK.class({
    
    _class: "Tagger",
    Extends: TK.Dialog,
    Implements: TK.Taggable,
    
    _options: Object.assign(Object.create(TK.Dialog.prototype._options), {
        closenew: "boolean",
        add: "boolean",
    }),
    options: {
        closenew: true,
        visible: false,
        add: true,
    },
    initialize: function (options) {
        TK.Dialog.prototype.initialize.call(this, options);
        TK.add_class(this.element, "toolkit-tagger");
        
        TK.Taggable.prototype.initialize.call(this);
        this.append_child(this.tags);
        this.add_event("addtag", new_tag_from_input.bind(this));
        
        this.set("add", this.options.add);
    },
    destroy: function (options) {
        TK.Taggable.prototype.destroy.call(this);
        TK.Dialog.prototype.destroy.call(this);
    },
    redraw: function () {
        TK.Dialog.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        if (I.add) {
            I.add = false;
            if (O.add) {
                if (!this._input) {
                    this._input = TK.element("input", "toolkit-input");
                    this._input.addEventListener("keyup", keyup.bind(this), true);
                    this._input.type = "text";
                    this._input.placeholder = "New tag";
                    this.element.appendChild(this._input);
                }
                this.add.set("container", this.element);
                TK.add_class(this.element, "toolkit-has-input");
            } else if (!O.add) {
                if (this._input) {
                    this.element.removeChild(this._input);
                    this._input = null;
                }
                this.add.set("container", false);
                TK.remove_class(this.element, "toolkit-has-input");
            }
        }
    },
    add_tag: function (tag, options) {
        var t = TK.Taggable.prototype.add_tag.call(this, tag, options);
        if (!t) return;
        t.node.label.add_event("click", (function (that, tag) {
            return function (e) {
                that.fire_event("tagclicked", tag.tag, tag.node);
            }
        })(this, t));
        if (this.options.visible)
          this.reposition();
        return t;
    },
    remove_tag: function (tag, node, purge) {
      TK.Taggable.prototype.remove_tag.call(this, tag, node, purge);
      if (!this.taglist.length)
        this.close();
      if (this.options.visible)
          this.reposition();
    },
});

})(this, this.TK);
