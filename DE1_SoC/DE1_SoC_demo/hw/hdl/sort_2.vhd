library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sort_2 is
port (a : in unsigned (31 downto 0) := X"00000000";
		b : in unsigned(31 downto 0) := X"00000000";
		sort_op_up : out unsigned(31 downto 0) := X"00000000";
		sort_op_down : out unsigned(31 downto 0) := X"00000000");
end sort_2;

architecture beh of sort_2 is
begin

	process(a,b)
	begin
		if (a > b) then
			sort_op_up <= b;
			sort_op_down <= a;
		else
			sort_op_down <= b;
			sort_op_up <= a;
		end if;
	end process;

end beh;