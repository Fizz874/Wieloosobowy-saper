using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MultiSaper
{
    internal class Player : IComparable
    {
        public int id = 0;
        public int score = 0;
        public string name = "";
        public Player() { }

        public Player(int id, int score, string name)
        {
            this.id = id;
            this.score = score;
            this.name = name;
        }

        public int CompareTo(object obj)
        {
            Player pl = (Player)obj;
            if (this.score > pl.score) return -1;
            if (this.score == pl.score) return String.Compare(this.name, pl.name, comparisonType: StringComparison.OrdinalIgnoreCase);
            return 1;
        }
    }
}
