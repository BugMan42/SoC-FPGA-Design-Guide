library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package bus_input_pkg is
        type bus_array is array(integer range <>) of unsigned (31 downto 0);
end package;