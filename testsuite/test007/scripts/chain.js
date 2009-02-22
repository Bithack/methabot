function test1()
{
    this.data = this.data.replace(/remove_me1/, "");

    println("test1:\n"+this.data+"\n");

    return null;
}

function test2()
{
    this.data = this.data.replace(/remove_me2/, "");

    println("test2:\n"+this.data+"\n");

    return null;
}

function test3()
{
    /* this.data should be valid XML now */

    var x = new XML(this.data);

    x = x..div;

    for each(y in x) {
        if (x.@id == "target") {
            this.data = y.toString();
            break;
        }
    }

    println("test3:\n"+this.data+"\n");

    return null;
}

